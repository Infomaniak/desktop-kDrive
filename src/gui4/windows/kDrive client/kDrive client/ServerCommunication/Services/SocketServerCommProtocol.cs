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
using System.Net.Sockets;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;
using static Infomaniak.kDrive.ServerCommunication.Interfaces.IServerCommProtocol;


namespace Infomaniak.kDrive.ServerCommunication.Services
{
    public class SocketServerCommProtocol : Interfaces.IServerCommProtocol
    {
        private Socket? _socket;
        private int _requestIdCounter = 0;
        private readonly byte[] _receiveBuffer = new byte[65536]; // 64 Ko
        private readonly StringBuilder _inBuffer = new();
        private readonly Decoder _decoder = Encoding.Unicode.GetDecoder();
        private int _inBufferJsonBalance = 0;
        private int _inBufferJsonBalanceSeen = 0;
        private readonly JsonSerializerOptions _jsonOptions = new JsonSerializerOptions
        {
            PropertyNameCaseInsensitive = true,
            Converters = { new Base64StringJsonConverter() }
        };
        private readonly ConcurrentDictionary<long, TaskCompletionSource<CommData>> _pendingRequests = [];
        private bool _stopRequested = false;
        private Task? _pollingTask;
        private const string _host = "127.0.0.1";
        private int NextId
        {
            get
            {
                if (_requestIdCounter >= int.MaxValue - 1)
                {
                    Logger.Log(Logger.Level.Info, "Request ID counter overflow, resetting to 0.");
                    _requestIdCounter = 0;
                }
                return ++_requestIdCounter;
            }
        }
        private string _commPortFilePath = Path.Combine(
               Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
               "kDrive",
               ".comm"
           );

        public event EventHandler<SignalEventArgs>? SignalReceived;
        public event EventHandler? ConnectionLost;

        ~SocketServerCommProtocol()
        {
            _socket?.Dispose();
            _stopRequested = true;
        }

        private int? GetServerPort()
        {
            try
            {
                int port = int.Parse(File.ReadAllText(_commPortFilePath).Trim());
                return port;
            }
            catch (FileNotFoundException)
            {
                return null;
            }
            catch (Exception ex)
            {
                Logger.Log(Logger.Level.Error, $"Failed to read server port from .comm file: {ex.Message}");
                return null;
            }
        }
        public async Task<bool> InitConnection(CancellationToken cancellationToken)
        {
            if (_pollingTask is not null)
            {
                Logger.Log(Logger.Level.Error, "Connection initialization attempted while polling task is already running.");
                return false;
            }

            while (!_stopRequested && !cancellationToken.IsCancellationRequested)
            {
                if (_socket is not null && _socket.Connected)
                    return true;

                int? port = GetServerPort();
                try
                {
                    if (port is null)
                    {
                        await Task.Delay(500, cancellationToken).ConfigureAwait(false);
                        continue;
                    }

                    Logger.Log(Logger.Level.Info, $"Attempting to connect to {_host}:{port}");
                    _socket?.Dispose();
                    _socket = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
                    await _socket.ConnectAsync(_host, port.Value, cancellationToken).ConfigureAwait(false);
                    Logger.Log(Logger.Level.Info, "Connected to server.");
                    _pollingTask = Task.Run(PollingLoop);
                    return true;
                }
                catch (OperationCanceledException)
                {
                    Logger.Log(Logger.Level.Info, "Connection initialization was canceled.");
                    return false;
                }
                catch (SocketException ex)
                {
                    Logger.Log(Logger.Level.Warning, $"Connection failed: {ex.Message}. Retrying in 0.5 seconds...");
                    _socket?.Dispose();
                    _socket = null;
                    await Task.Delay(500, cancellationToken).ConfigureAwait(false);
                    continue;
                }
            }
            return false;
        }
        private async Task PollingLoop()
        {
            if (_socket is null)
                return;

            while (!_stopRequested && _socket?.Connected == true)
            {
                try
                {
                    if (!_socket.Poll(TimeSpan.FromSeconds(5), SelectMode.SelectRead))
                        continue;
                }
                catch (SocketException ex)
                {
                    Logger.Log(Logger.Level.Error, $"Socket poll error: {ex.Message}");
                    _socket?.Dispose();
                    _socket = null;
                    ConnectionLost?.Invoke(this, EventArgs.Empty);
                    return;
                }

                if (_socket.Available == 0)
                {
                    Logger.Log(Logger.Level.Warning, "Server has closed the connection.");
                    _socket?.Dispose();
                    _socket = null;
                    ConnectionLost?.Invoke(this, EventArgs.Empty);
                    return;
                }

                try
                {
                    await OnReadyReadAsync().ConfigureAwait(false);
                }
                catch (Exception ex)
                {
                    Logger.Log(Logger.Level.Error, $"Error processing incoming message: {ex.Message}");
                    // Continue the loop to keep the connection alive, unless it's a critical error
                }
            }
        }

        public async Task<CommData> SendRequestAsync(RequestNum requestNum, JsonObject parameters, CancellationToken cancellationToken = default)
        {
            try
            {
                // Wait asynchronously until the client is connected
                const int softWaitTimeMs = 30000; // Log every 30s
                var lastLogTime = Stopwatch.StartNew();
                while (_socket == null || !_socket.Connected)
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

                await _socket.SendAsync(jsonBytes).ConfigureAwait(false);
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
                ConnectionLost?.Invoke(this, EventArgs.Empty);
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

        private bool CheckBufferConsistency()
        {
            if (_inBuffer.Length == 0)
                return true;
            if (_inBuffer[0] != '{')
            {
                Logger.Log(Logger.Level.Error, "Invalid message format: does not start with '{'.");
                return false;
            }
            return true;
        }

        private async Task OnReadyReadAsync()
        {
            do
            {
                if (_socket == null || !_socket.Connected)
                {
                    Logger.Log(Logger.Level.Warning, "Unable to read: socket is not connected.");
                    return;
                }

                // Read the JSON message
                int jsonEndIndex = -1;

                if (_socket.Available > 0)
                {
                    int bytesRead = await _socket.ReceiveAsync(_receiveBuffer).ConfigureAwait(false);
                    int charCount = _decoder.GetCharCount(_receiveBuffer, 0, bytesRead, flush: false);
                    char[] chars = new char[charCount];
                    _decoder.GetChars(_receiveBuffer, 0, bytesRead, chars, 0, flush: false);
                    _inBuffer.Append(chars, 0, charCount);
                }

                if (!CheckBufferConsistency())
                {
                    ConnectionLost?.Invoke(this, EventArgs.Empty);
                    return;
                }

                UpdateJsonBalance(_inBuffer, ref _inBufferJsonBalanceSeen, ref _inBufferJsonBalance, ref jsonEndIndex);
               
                if (jsonEndIndex == -1)
                {
                    Logger.Log(Logger.Level.Extended, "Incomplete JSON message, waiting for more data.");
                    return;
                }

                ReadOnlySpan<char> jsonSpan = _inBuffer.ToString(0, jsonEndIndex + 1);
                if (!jsonSpan.EndsWith("}"))
                {
                    Logger.Log(Logger.Level.Error, "Unexpected end character");
                    ConnectionLost?.Invoke(this, EventArgs.Empty);
                    return;
                }

                _inBuffer.Remove(0, jsonEndIndex + 1);
                _inBufferJsonBalanceSeen = 0;

                // Parse JSON
                var messageObj = JsonSerializer.Deserialize<CommData>(jsonSpan, _jsonOptions);
                if (messageObj is null)
                {
                    Logger.Log(Logger.Level.Error, "Invalid message format.");
                    ConnectionLost?.Invoke(this, EventArgs.Empty);
                    return;
                }

                HandleServerMessageAsync(messageObj);
            } while (_socket.Available > 0 || _inBuffer.Length > 0);
        }

        private static void UpdateJsonBalance(StringBuilder sb, ref int seenIndex, ref int balance, ref int jsonEndIndex)
        {
            jsonEndIndex = -1;
            for (int i = seenIndex; i < sb.Length; i++)
            {
                char c = sb[i];
                if (c == '{') balance++;
                else if (c == '}')
                {
                    balance--;
                    if (balance == 0)
                    {
                        jsonEndIndex = i;
                        seenIndex = i;
                        break;
                    }
                }
            }
            seenIndex = sb.Length;
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
