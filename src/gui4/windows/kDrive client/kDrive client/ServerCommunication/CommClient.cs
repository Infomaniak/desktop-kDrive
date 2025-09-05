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

using Microsoft.Extensions.Logging;
using System;
using System.Buffers.Binary;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Collections.Immutable;
using System.ComponentModel.DataAnnotations;
using System.IO;
using System.Linq;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Windows.Media.Core;
using WinRT;


// This first implementation is a basic one, it just connects to the server and reads incoming messages.
// A final implementation should be done once the new communication layer on the server side is ready.
namespace Infomaniak.kDrive.ServerCommunication
{
    public class CommClient
    {
        private readonly TcpClient? _client;
        private readonly Dictionary<int, Func<int/*Id*/, ImmutableArray<byte>/*parms*/, bool>> _signalHandlers = new();
        private long _idCounter = 0;

        private readonly ConcurrentDictionary<long, CommData?> pendingRequests = new ConcurrentDictionary<long, CommData?>();
        private long nextId
        {
            get => ++_idCounter;
        }

        public CommClient()
        {
            // Initialize signal handlers
            _signalHandlers[15] = (id, parms) => { return onSyncProgressInfo(id, parms); };


            // Fetch the port from the .comm file
            string homePath = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile) + Path.DirectorySeparatorChar + ".comm";
            int port = Int32.Parse(File.ReadAllText(homePath).Trim());

            Logger.Log(Logger.Level.Info, $"Connecting to port {port}");
            // Prefer a using declaration to ensure the instance is Disposed later.
            try
            {
                _client = new TcpClient("localhost", port);
                Logger.Log(Logger.Level.Info, "Connected to server.");
            }
            catch (Exception ex)
            {
                Logger.Log(Logger.Level.Error, $"Socket connection error: {ex.Message}");
                _client = null;
            }

            if (_client != null && _client.Connected)
            {
                Task.Run(async () =>
                {
                    while (true)
                    {
                        try
                        {
                            while (_client.GetStream().DataAvailable)
                            {
                                onReadyRead();
                            }
                            await Task.Delay(100).ConfigureAwait(false); // Polling interval
                        }
                        catch (Exception ex)
                        {
                            Logger.Log(Logger.Level.Error, $"Error in read loop: {ex.Message}");
                            break;
                        }
                    }
                });
            }
            else
            {
                // TODO: Implement a retry mechanism to connect to the server (waiting for it to be ready)
                Logger.Log(Logger.Level.Error, "Client is null or not connected, read loop not started.");
            }
        }

        ~CommClient()
        {
            _client?.Dispose();
        }

        public async Task<CommData> SendRequest(int num, ImmutableArray<byte> parms)
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

                long requestId = nextId;
                pendingRequests[requestId] = null;

                // Create the JSON object
                var requestObj = new Dictionary<string, object>
                {
                    ["type"] = 0,
                    ["id"] = requestId,
                    ["num"] = num,
                    ["params"] = Convert.ToBase64String(parms.ToArray())
                };

                // Convert to JSON
                string jsonString = System.Text.Json.JsonSerializer.Serialize(requestObj);
                byte[] jsonBytes = System.Text.Encoding.UTF8.GetBytes(jsonString);

                // Get the size as bytes (4-byte integer)
                byte[] sizeBytes = new byte[4];
                BinaryPrimitives.WriteInt32BigEndian(sizeBytes, jsonBytes.Length); // Currently the server expects big-endian, once communication layer is ready it should be changed to little-endian

                // Write size followed by JSON data
                // TODO: When writing final version, ensure thread safety and handle exceptions
                NetworkStream stream = _client.GetStream();
                await stream.WriteAsync(sizeBytes, 0, sizeBytes.Length).ConfigureAwait(false);
                await stream.WriteAsync(jsonBytes, 0, jsonBytes.Length).ConfigureAwait(false);

                Logger.Log(Logger.Level.Info, $"Sent request: {jsonString}");
                CommData reply = await waitForReplyAsync(requestId).ConfigureAwait(false);
                return reply;
            }
            catch (OperationCanceledException)
            {
                Logger.Log(Logger.Level.Error, "Request timed out.");
                return new CommData();
            }
            catch (Exception ex)
            {
                Logger.Log(Logger.Level.Error, $"Socket write error: {ex.Message}");
                return new CommData();
            }
        }
        private async Task<CommData> waitForReplyAsync(long requestId)
        {
            // Ensure the slot exists
            pendingRequests[requestId] = null;

            while (true)
            {
                if (pendingRequests.TryGetValue(requestId, out var reply))
                {
                    if (reply != null)
                    {
                        pendingRequests.Remove(requestId, out _);
                        return reply;
                    }
                    else
                    {
                        Logger.Log(Logger.Level.Debug, $"Found request(Id) {requestId}, but the respons is not available yet null");
                    }
                }
                else
                {
                    Logger.Log(Logger.Level.Debug, $"No request(Id) found for {requestId}");
                }
                await Task.Delay(10).ConfigureAwait(false);
            }
        }

        private void onReadyRead()
        {
            if (_client == null || !_client.Connected)
            {
                Logger.Log(Logger.Level.Warning, "Unable to read: client is not connected.");
                return;
            }
            NetworkStream stream = _client.GetStream();
            Logger.Log(Logger.Level.Debug, "Data is ready to be read from the server.");

            // Read the size of the incoming message (4 bytes)
            byte[] sizeBytes = new byte[4];
            int bytesRead = stream.Read(sizeBytes, 0, sizeBytes.Length);
            if (bytesRead < 4)
            {
                Logger.Log(Logger.Level.Warning, "Incomplete size header received.");
                return;
            }
            int messageSize = BinaryPrimitives.ReadInt32BigEndian(sizeBytes); // Currently the server sends big-endian, once communication layer is ready it should be changed to little-endian
            Logger.Log(Logger.Level.Debug, $"Message size: {messageSize} bytes.");

            // Read the JSON message
            byte[] jsonBytes = new byte[messageSize];
            bytesRead = stream.Read(jsonBytes, 0, jsonBytes.Length);
            if (bytesRead < messageSize)
            {
                Logger.Log(Logger.Level.Warning, "Incomplete message received.");
                return;
            }
            string jsonString = System.Text.Encoding.UTF8.GetString(jsonBytes);
            Logger.Log(Logger.Level.Debug, $"Received message: {jsonString}");

            // Parse JSON
            var messageObj = System.Text.Json.JsonSerializer.Deserialize<CommData>(jsonString);
            if (messageObj == null)
            {
                Logger.Log(Logger.Level.Warning, "Invalid message format.");
                return;
            }
            handleServerMessage(messageObj);
        }

        private void handleServerMessage(CommData data)
        {
            Logger.Log(Logger.Level.Info, $"Message type: {data.type}, id: {data.id}, num: {data.num}");
            switch (data.type)
            {
                case 0:
                    Logger.Log(Logger.Level.Warning, "Received a request from server.");
                    // TODO: Handle requests
                    break;
                case 1:
                    Logger.Log(Logger.Level.Info, $"Received a reply for id {data.id} with result size: {data.ResultByteArray.Length} bytes.");
                    pendingRequests[data.id] = data;
                    break;
                case 2:
                    // Signal
                    if (_signalHandlers.ContainsKey(data.num))
                    {
                        bool res = _signalHandlers[data.num](data.id, data.ParmsByteArray);
                        Logger.Log(Logger.Level.Info, $"Signal handler for id {data.id} executed with result: {res}");
                    }
                    else
                    {
                        Logger.Log(Logger.Level.Warning, $"No handler registered for signal num {data.num}");
                    }
                    break;
                default:
                    Logger.Log(Logger.Level.Warning, $"Unknown message type: {data.type}");
                    break;
            }
        }

        private bool onSyncProgressInfo(int id, ImmutableArray<byte> parms)
        {
            Logger.Log(Logger.Level.Debug, "Sync progress info signal received.");
            // TODO: Implement actual handling logic here
            return true;
        }
    }
}
