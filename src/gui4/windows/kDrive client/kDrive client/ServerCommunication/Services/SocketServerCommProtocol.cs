/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

using Infomaniak.kDrive.ServerCommunication.JsonConverters;
using Infomaniak.kDrive.ViewModels;
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Collections.Immutable;
using System.IO;
using System.Linq;
using System.Net.Sockets;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;
using static Infomaniak.kDrive.ServerCommunication.CommShared;
using static Infomaniak.kDrive.ServerCommunication.Interfaces.IServerCommProtocol;


namespace Infomaniak.kDrive.ServerCommunication.Services
{
    // For the moment, non implemented methods will fallback to the MockServerCommProtocol implementation
    public class SocketServerCommProtocol : MockServerCommProtocol, Interfaces.IServerCommProtocol
    {
        private TcpClient? _client;
        private long _requestIdCounter = 0;
        private string _inBuffer = "";
        private int _inBufferJsonBalance = 0;
        private readonly ConcurrentDictionary<long, CommData?> _pendingRequests = new ConcurrentDictionary<long, CommData?>();
        private long NextId
        {
            get => ++_requestIdCounter;
        }

        public new event EventHandler<SignalEventArgs>? SignalReceived;

        public SocketServerCommProtocol()
        {
            base.SignalReceived += (s, e) => SignalReceived?.Invoke(s, e); // Forward signals from base class

            Initialize();
        }

        public new void Initialize()
        {
            // Fetch the port from the .comm4 file
            // TODO: Decide where to store this file on Windows
            string homePath = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile) + Path.DirectorySeparatorChar + ".comm4";
            int port = int.Parse((File.ReadAllText(homePath)).Trim());


            Logger.Log(Logger.Level.Info, $"Connecting to port {port}");
            try
            {
                _client = new TcpClient("localhost", port);
                Logger.Log(Logger.Level.Info, "Connected to server.");
            }
            catch (SocketException ex)
            {
                Logger.Log(Logger.Level.Error, $"Socket connection error: {ex.Message}");
                _client = null;
            }

            if (_client != null && _client.Connected)
            {
                _ = Task.Run(async () =>
                {
                    while (true)
                    {
                        try
                        {
                            while (_client.GetStream().DataAvailable || _inBuffer.Any())
                            {
                                await OnReadyReadAsync().ConfigureAwait(false);
                            }
                            await Task.Delay(100).ConfigureAwait(false); // Polling interval
                        }
                        catch (Exception ex)
                        {
                            Logger.Log(Logger.Level.Error, $"Error in read loop: {ex.Message}");
                        }
                    }
                });
            }
            else
            {
                Logger.Log(Logger.Level.Error, "Client is null or not connected, read loop not started.");
            }
        }

        ~SocketServerCommProtocol()
        {
            _client?.Dispose();
        }

        public new async Task<CommData> SendRequestAsync(RequestNum requestNum, JsonObject parameters, CancellationToken cancellationToken = default)
        {
            try
            {
                if (_client == null)
                {
                    Logger.Log(Logger.Level.Warning, "Unable to send request: client is null.");
                    return new CommData();
                }

                if (!_client.Connected)
                {
                    Logger.Log(Logger.Level.Warning, "Unable to send request: client is not connected.");
                    return new CommData();
                }

                long requestId = NextId;
                _pendingRequests[requestId] = null;

                // Create the JSON object
                var requestObj = new JsonObject
                {
                    ["type"] = 0,
                    ["id"] = requestId,
                    ["num"] = (int)requestNum,
                    ["params"] = parameters
                };

                // Convert to JSON
                string jsonString = JsonSerializer.Serialize(requestObj);
                byte[] jsonBytes = Encoding.Unicode.GetBytes(jsonString);

                NetworkStream stream = _client.GetStream();
                await stream.WriteAsync(jsonBytes, 0, jsonBytes.Length, cancellationToken).ConfigureAwait(false);
                Logger.Log(Logger.Level.Info, $"Sent request: {jsonString}");
                cancellationToken.ThrowIfCancellationRequested();
                CommData reply = await WaitForReplyAsync(requestId, cancellationToken).ConfigureAwait(false);
                if(reply.RequestNum == RequestNum.Unknown)
                {
                    Logger.Log(Logger.Level.Debug, "Request not implemented in the server, trying the mock implementation.");
                    reply = await base.SendRequestAsync(requestNum, parameters, cancellationToken).ConfigureAwait(false);
                }
                return reply;
            }
            catch (OperationCanceledException)
            {
                Logger.Log(Logger.Level.Error, "Request operation was canceled.");
                return new CommData();
            }
            catch (Exception ex)
            {
                Logger.Log(Logger.Level.Error, $"Socket write error: {ex.Message}");
                return new CommData();
            }
        }
       
        private async Task<CommData> WaitForReplyAsync(long requestId, CancellationToken cancellationToken = default)
        {
            // Ensure the slot exists
            _pendingRequests[requestId] = null;

            while (true)
            {
                if (_pendingRequests.TryGetValue(requestId, out var reply))
                {
                    if (reply != null)
                    {
                        _pendingRequests.Remove(requestId, out _);
                        return reply;
                    }
                    else
                    {
                        Logger.Log(Logger.Level.Debug, $"Found request(Id) {requestId}, but the response is not available yet null");
                    }
                }
                else
                {
                    Logger.Log(Logger.Level.Debug, $"No request(Id) found for {requestId}");
                }
                await Task.Delay(10).ConfigureAwait(false);
                if (cancellationToken.IsCancellationRequested)
                {
                    Logger.Log(Logger.Level.Info, $"Request(Id) {requestId} canceled before completion.");
                    _pendingRequests.Remove(requestId, out _);
                    cancellationToken.ThrowIfCancellationRequested();
                }
            }
        }

        private async Task OnReadyReadAsync()
        {
            if (_client == null || !_client.Connected)
            {
                Logger.Log(Logger.Level.Warning, "Unable to read: client is not connected.");
                return;
            }
            NetworkStream stream = _client.GetStream();
            Logger.Log(Logger.Level.Debug, "Data is ready to be read from the server.");

            // Read the JSON message
            int jsonEndIndex = -1;

            if (_client.GetStream().DataAvailable)
            {
                byte[] jsonBytes = new byte[1024 * 4];
                int bytesRead = await stream.ReadAsync(jsonBytes, 0, 1024 * 4).ConfigureAwait(false);

                string str = Encoding.Unicode.GetString(jsonBytes, 0, bytesRead);
                _inBuffer += str;

                int endIndex = -1;
                UpdateJsonBalance(str, ref _inBufferJsonBalance, ref endIndex);
                if (endIndex != -1)
                    jsonEndIndex = _inBuffer.Length - str.Length + endIndex;
                else
                    jsonEndIndex = -1;
            }
            else
            {
                UpdateJsonBalance(_inBuffer, ref _inBufferJsonBalance, ref jsonEndIndex);
            }

            // Consistency check
            if (_inBuffer.Any() && _inBuffer.First() != '{')
            {
                Logger.Log(Logger.Level.Warning, "Invalid message format: does not start with '{'.");
                _inBuffer = ""; // Discard the buffer to resync
                _inBufferJsonBalance = 0;
                return;
            }
            if (jsonEndIndex == -1)
            {
                Logger.Log(Logger.Level.Debug, "Incomplete JSON message, waiting for more data.");
                return; // Wait for more data
            }

            string jsonString = _inBuffer.Substring(0, jsonEndIndex + 1);
            _inBuffer = _inBuffer.Substring(jsonEndIndex + 1);

            // Parse JSON
            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            var messageObj = JsonSerializer.Deserialize<CommData>(jsonString, options);
            if (messageObj == null)
            {
                Logger.Log(Logger.Level.Warning, "Invalid message format.");
                return;
            }
            HandleServerMessageAsync(messageObj);
        }

        private static void UpdateJsonBalance(string str, ref int balance, ref int endIndex)
        {
            endIndex = -1;
            for (int i = 0; i < str.Length; i++)
            {
                if (str[i] == '{')
                    balance++;
                else if (str[i] == '}')
                    balance--;
                if (balance == 0)
                {
                    endIndex = i;
                    break;
                }
            }
        }

        private void HandleServerMessageAsync(CommData data)
        {
            Logger.Log(Logger.Level.Info, $"Message type: {data.Type}, id: {data.Id}, num: {data.Num}");
            switch (data.Type)
            {
                case CommMessageType.Request:
                    Logger.Log(Logger.Level.Info, $"Received a reply for id {data.Id}");
                    _pendingRequests[data.Id] = data;
                    break;
                case CommMessageType.Signal:
                    // Signal
                    SignalNum signalNum = (SignalNum)data.Num;
                    RaiseSignal(signalNum, data.Params);
                    break;
                default:
                    Logger.Log(Logger.Level.Warning, $"Unknown message type: {data.Type}");
                    break;
            }
        }

        private void RaiseSignal(SignalNum signalNum, JsonObject parameters)
        {
            SignalReceived?.Invoke(this, new SignalEventArgs(signalNum, parameters));
        }
    }
}
