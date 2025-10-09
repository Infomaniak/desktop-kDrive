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
    public class MockServerCommProtocol : Interfaces.IServerCommProtocol
    {
        private long _requestIdCounter = 0;
        private long NextId
        {
            get => ++_requestIdCounter;
        }
        private MockData.MockServerData _mockData = new MockData.MockServerData();

        public event EventHandler<SignalEventArgs>? SignalReceived;
        private Queue<KeyValuePair<SignalNum, JsonObject>> PendingSignals { get; } = new Queue<KeyValuePair<SignalNum, JsonObject>>();
        private Task? _signalHandler;
        public MockServerCommProtocol()
        {
            _ = Initialize();
        }

        public async Task<CommData> SendRequestAsync(RequestNum requestNum, JsonObject parameters, CancellationToken cancellationToken = default)
        {
            switch (requestNum)
            {
                case RequestNum.LoginRequestToken:
                    return await LoginRequestToken(parameters);
                case RequestNum.UserDbIds:
                    return await UserDbIdsRequest(parameters);
                case RequestNum.UserInfoList:
                    return await UserInfoListRequest(parameters);
                case CommShared.RequestNum.AccountInfoList:
                    return await AccountInfoListRequest(parameters);
                case RequestNum.DriveInfoList:
                    return await DriveInfoListRequest(parameters);
                case RequestNum.SyncInfoList:
                    return await SyncInfoListRequest(parameters);
                default:
                    throw new NotImplementedException($"RequestNum {requestNum} not implemented in MockServerCommProtocol.");
            }
        }

        private async Task<CommData> LoginRequestToken(JsonObject parameters)
        {
            var random = new Random();
            var randomUserIndex = random.Next(0, _mockData.Users.Count);
            var user = _mockData.Users.ElementAtOrDefault(randomUserIndex);
            if (user == null)
            {
                throw new InvalidOperationException("No users available in mock data.");
            }

            // Simulate UserAdded signal
            var userData = new JsonObject
            {
                { "userInfo", new JsonObject
                {
                    { "userDbId", user.DbId },
                    { "userId", user.UserId },
                    { "name", user.Name },
                    { "email", user.Email },
                    { "isConnected", user.IsConnected },
                    { "isStaff", user.IsStaff }
                }}
            };
            EnqueueSignal(SignalNum.UserAdded, userData);

            return new CommData
            {
                Type = CommMessageType.Request,
                Id = (int)NextId,
                RequestNum = RequestNum.LoginRequestToken,
                Params = new JsonObject
                {
                    { "userDbId", user.DbId }
                }
            };
        }

        private async Task<CommData> UserDbIdsRequest(JsonObject parameters)
        {
            List<DbId> userDbIds = _mockData.Users.Select(u => u.DbId).ToList();
            return new CommData
            {
                Type = CommMessageType.Request,
                Id = (int)NextId,
                RequestNum = RequestNum.UserDbIds,
                Params = new JsonObject
                {
                    { "userDbIds", JsonSerializer.SerializeToNode(userDbIds) }
                }
            };
        }

        private async Task<CommData> UserInfoListRequest(JsonObject parameters)
        {
            var users = _mockData.Users;

            JsonObject result = new JsonObject();
            foreach (var user in users)
            {
                var userData = new JsonObject
                {
                    { "dbId", user.DbId },
                    { "userId", user.UserId },
                    { "name", Convert.ToBase64String(Encoding.UTF8.GetBytes(user.Name)) },
                    { "email", Convert.ToBase64String(Encoding.UTF8.GetBytes(user.Email)) },
                    { "isConnected", user.IsConnected },
                    { "isStaff", user.IsStaff }
                };
                if (!result.ContainsKey("userInfo"))
                {
                    result["userInfo"] = new JsonArray();
                }
                ((JsonArray)result["userInfo"]!).Add(userData);

            }

            return new CommData
            {
                Type = CommMessageType.Request,
                Id = (int)NextId,
                RequestNum = RequestNum.UserInfoList,
                Params = result
            };
        }

        private async Task<CommData> AccountInfoListRequest(JsonObject parameters)
        {
            var accounts = _mockData.Accounts;

            JsonObject result = new JsonObject();
            foreach (var account in accounts)
            {
                var accountData = new JsonObject
                {
                    { "dbId", account.DbId },
                    { "UserDbId", account.User.DbId },
                                    };
                if (!result.ContainsKey("accountInfo"))
                {
                    result["accountInfo"] = new JsonArray();
                }
                ((JsonArray)result["accountInfo"]!).Add(accountData);

            }

            return new CommData
            {
                Type = CommMessageType.Request,
                Id = (int)NextId,
                RequestNum = RequestNum.AccountInfoList,
                Params = result
            };
        }

        private async Task<CommData> DriveInfoListRequest(JsonObject parameters)
        {
            var drives = _mockData.Drives;

            JsonObject result = new JsonObject();
            foreach (var drive in drives)
            {
                var driveData = new JsonObject
                {
                    { "dbId", drive.DbId },
                    { "driveId", drive.DriveId },
                    { "accountDbId", drive.Account.DbId },
                    { "name", Convert.ToBase64String(Encoding.UTF8.GetBytes(drive.Name)) },
                    { "color", drive.Color.ToArgb() },
                    { "notifications", true },
                    { "maintenance", false },
                    { "locked", false },
                    { "accessDenied", false }
                };
                if (!result.ContainsKey("driveInfo"))
                {
                    result["driveInfo"] = new JsonArray();
                }
                ((JsonArray)result["driveInfo"]!).Add(driveData);
            }

            return new CommData
            {
                Type = CommMessageType.Request,
                Id = (int)NextId,
                RequestNum = RequestNum.DriveInfoList,
                Params = result
            };
        }

        private async Task<CommData> SyncInfoListRequest(JsonObject parameters)
        {
            var syncs = _mockData.Syncs;

            JsonObject result = new JsonObject();
            foreach (var sync in syncs)
            {
                var syncData = new JsonObject
                {
                    { "dbId", sync.DbId },
                    { "driveDbId", sync.Drive.DbId },
                    { "localPath", Convert.ToBase64String(Encoding.UTF8.GetBytes(sync.LocalPath)) },
                    { "targetPath", Convert.ToBase64String(Encoding.UTF8.GetBytes(sync.RemotePath)) },
                    { "targetNodeId", "??????" },
                    { "supportOnlineMode", sync.SupportOnlineMode },
                    { "syncType", (int)sync.SyncType }
                };
                if (!result.ContainsKey("syncInfo"))
                {
                    result["syncInfo"] = new JsonArray();
                }
                ((JsonArray)result["syncInfo"]!).Add(syncData);

            }

            return new CommData
            {
                Type = CommMessageType.Request,
                Id = (int)NextId,
                RequestNum = RequestNum.SyncInfoList,
                Params = result
            };
        }

        private void EnqueueSignal(SignalNum signalNum, JsonObject parameters)
        {
            PendingSignals.Enqueue(new KeyValuePair<SignalNum, JsonObject>(signalNum, parameters));
        }

        private void RaiseSignal(SignalNum signalNum, JsonObject parameters)
        {
            SignalReceived?.Invoke(this, new SignalEventArgs(signalNum, parameters));
        }

        public Task Initialize()
        {
            _signalHandler = Task.Run(async () =>
            {
                while (true)
                {
                    while (PendingSignals.Count > 0)
                    {
                        var signal = PendingSignals.Dequeue();
                        RaiseSignal(signal.Key, signal.Value);
                    }
                    await Task.Delay(100);
                }
            });
            return Task.CompletedTask;
        }
    }
}
