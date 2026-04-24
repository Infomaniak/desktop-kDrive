/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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
using Infomaniak.kDrive.Types;
using System;
using System.Collections.Concurrent;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net.Sockets;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;
using static Infomaniak.kDrive.ServerCommunication.Interfaces.IServerCommProtocol;


namespace Infomaniak.kDrive.ServerCommunication.Services
{
    // For the moment, non implemented methods will fallback to the MockServerCommProtocol implementation
    public class SocketServerCommProtocol : Interfaces.IServerCommProtocol
    {
        private TcpClient? _client;
        private long _requestIdCounter = 0;
        private string _inBuffer = "";
        private int _inBufferJsonBalance = 0;
        private readonly ConcurrentDictionary<long, TaskCompletionSource<CommData>> _pendingRequests = [];
        private bool _stopRequested = false;
        private const string _host = "127.0.0.1";
        private long NextId
        {
            get
            {
                if (_requestIdCounter >= long.MaxValue - 1)
                {
                    Logger.Log(Logger.Level.Info, "Request ID counter overflow, resetting to 0.");
                    _requestIdCounter = 0;
                }
                return ++_requestIdCounter;
            }
        }

        public event EventHandler<SignalEventArgs>? SignalReceived;
        public event EventHandler? ConnectionLost;

        public SocketServerCommProtocol()
        {
            Initialize();
        }

        public Task Initialize()
        {
            Task.Run(ReconnectLoop);
            return Task.CompletedTask;
        }

        ~SocketServerCommProtocol()
        {
            _client?.Dispose();
            _stopRequested = true;
        }

        private async Task ReconnectLoop()
        {
            string commPortFilePath = Path.Combine(
               Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
               "kDrive",
               ".comm"
           );

            while (!_stopRequested)
            {
                if (_client == null || !_client.Connected)
                {
                    try
                    {
                        int port = int.Parse(File.ReadAllText(commPortFilePath).Trim());

                        Logger.Log(Logger.Level.Info, $"Attempting to connect to {_host}:{port}");
                        _client?.Dispose();
                        _client = new TcpClient();
                        await _client.ConnectAsync(_host, port).ConfigureAwait(false);
                        Logger.Log(Logger.Level.Info, "Connected to server.");
                    }
                    catch (SocketException ex)
                    {
                        Logger.Log(Logger.Level.Warning, $"Connection failed: {ex.Message}. Retrying in 2 seconds...");
                        _client = null;
                        await Task.Delay(2000).ConfigureAwait(false);
                        continue;
                    }
                    catch (FileNotFoundException)
                    {
                        Logger.Log(Logger.Level.Error, $".comm file not found at {commPortFilePath}. Retrying in 2 seconds...");
                        await Task.Delay(2000).ConfigureAwait(false);
                        continue;
                    }
                }

                try
                {
                    while (_client.Connected)
                    {
                        while (_client.GetStream().DataAvailable || _inBuffer.Any())
                        {
                            await OnReadyReadAsync().ConfigureAwait(false);
                        }
                        await Task.Delay(100).ConfigureAwait(false); // Polling interval
                    }
                }
                catch (Exception ex)
                {
                    Logger.Log(Logger.Level.Error, $"Error in read loop: {ex.Message}");
                    _client?.Dispose();
                    _client = null;
                }

                Logger.Log(Logger.Level.Warning, "Disconnected from server, attempting to reconnect...");
                ConnectionLost?.Invoke(this, EventArgs.Empty);
                // Small delay before attempting reconnect
                await Task.Delay(2000).ConfigureAwait(false);
            }
        }

        public async Task<CommData> SendRequestAsync(RequestNum requestNum, JsonObject parameters, CancellationToken cancellationToken = default)
        {
            try
            {
                // Wait asynchronously until the client is connected
                const int softWaitTimeMs = 30000; // Log every 30s
                var lastLogTime = Stopwatch.StartNew();
                while (_client == null || !_client.Connected)
                {
                    cancellationToken.ThrowIfCancellationRequested();

                    if (lastLogTime.ElapsedMilliseconds > softWaitTimeMs)
                    {
                        Logger.Log(Logger.Level.Warning, "Client not connected, waiting to send request...");
                        lastLogTime.Restart();
                    }
                    await Task.Delay(100, cancellationToken).ConfigureAwait(false);
                }


                long requestId = NextId;
                _pendingRequests[requestId] = new TaskCompletionSource<CommData>(
                    TaskCreationOptions.RunContinuationsAsynchronously);

                // Create the JSON object
                var requestObj = new JsonObject
                {
                    ["type"] = (int)CommMessageType.Request,
                    ["id"] = requestId,
                    ["num"] = (int)requestNum,
                    ["params"] = parameters.Deserialize<JsonNode>()
                };

                // Convert to JSON
                string jsonString = JsonSerializer.Serialize(requestObj);
                byte[] jsonBytes = Encoding.Unicode.GetBytes(jsonString);

                NetworkStream stream = _client.GetStream();
                await stream.WriteAsync(jsonBytes, 0, jsonBytes.Length, cancellationToken).ConfigureAwait(false);
                Logger.Log(Logger.Level.Info, $"Sent request: {jsonString}");
                cancellationToken.ThrowIfCancellationRequested();
                CommData reply = await WaitForReplyAsync(requestId, cancellationToken).ConfigureAwait(false);
                if (reply.RequestNum == RequestNum.Unknown)
                {
                    Logger.Log(Logger.Level.Debug, "Request not implemented server-side");
                }
                return reply;
            }
            catch (OperationCanceledException)
            {
                Logger.Log(Logger.Level.Info, "Request operation was canceled.");
                return new CommData();
            }
            catch (Exception ex)
            {
                Logger.Log(Logger.Level.Error, $"Socket write error: {ex.Message}");
                return new CommData();
            }
        }
        private Task<CommData> WaitForReplyAsync(long requestId, CancellationToken ct = default)
        {

            if (!_pendingRequests.TryGetValue(requestId, out var tcs))
            {
                Logger.Log(Logger.Level.Error, $"RequestId {requestId} not found in pending requests.");
                return Task.FromResult(new CommData());
            }

            // Tie cancellation to the task
            if (ct.CanBeCanceled)
            {
                ct.Register(() =>
                {
                    if (_pendingRequests.TryRemove(requestId, out var pending))
                        pending.TrySetCanceled(ct);
                });
            }

            return tcs.Task;
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
                if (_inBufferJsonBalance != 0)
                {
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
            }
            else
            {
                _inBufferJsonBalance = 0;
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
            if (!jsonString.EndsWith("}"))
            {
                Logger.Log(Logger.Level.Error, "unexpected end character");
            }
            _inBuffer = _inBuffer.Substring(jsonEndIndex + 1);

            // Parse JSON
            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            Logger.Log(Logger.Level.Debug, $"Deserializing: {jsonString}");
            var messageObj = JsonSerializer.Deserialize<CommData>(jsonString, options);
            if (messageObj is null)
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
                    if (_pendingRequests.TryRemove(data.Id, out var tcs))
                    {
                        tcs.TrySetResult(data);
                    }
                    else
                    {
                        Logger.Log(Logger.Level.Warning, $"Received reply for unknown request ID {data.Id}");
                    }
                    break;
                case CommMessageType.Signal:
                    // Signal
                    SignalNum signalNum = (SignalNum)data.Num;
                    RaiseSignal(signalNum, data.Params ?? []);
                    break;
                default:
                    Logger.Log(Logger.Level.Warning, $"Unknown message type: {data.Type}");
                    break;
            }
        }

        protected void RaiseSignal(SignalNum signalNum, JsonObject parameters)
        {
            SignalReceived?.Invoke(this, new SignalEventArgs(signalNum, parameters));
        }
    }
}
