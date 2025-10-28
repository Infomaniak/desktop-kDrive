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

using Infomaniak.kDrive.ServerCommunication.CommStruct;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;
using static Infomaniak.kDrive.ServerCommunication.Interfaces.IServerCommProtocol;

namespace Infomaniak.kDrive.ServerCommunication.Services
{
    public class MockServerCommProtocol : Interfaces.IServerCommProtocol
    {
        private MockServerData _mockData = new MockServerData();

        private long _requestIdCounter = 0;
        private long NextId
        {
            get => ++_requestIdCounter;
        }

        public event EventHandler<SignalEventArgs>? SignalReceived;
        private Queue<KeyValuePair<SignalNum, JsonObject>> PendingSignals { get; } = new Queue<KeyValuePair<SignalNum, JsonObject>>();
        private Task? _signalHandler;
        private Task? _customSignalsHandler;
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
                case RequestNum.AccountInfoList:
                    return await AccountInfoListRequest(parameters);
                case RequestNum.DriveInfoList:
                    return await DriveInfoListRequest(parameters);
                case RequestNum.SyncInfoList:
                    return await SyncInfoListRequest(parameters);
                case RequestNum.UPDATER_START_INSTALLER:
                    return await UpdateStartInstaller(parameters);
                case RequestNum.UPDATER_VERSION_INFO:
                    return await UpdaterVersionInfo(parameters);
                case RequestNum.UPDATER_CHANGE_CHANNEL:
                    return await UpdaterChangeChannel(parameters);
                case RequestNum.PARAMETERS_INFO:
                    return await ParametersInfo(parameters);
                default:
                    throw new NotImplementedException($"RequestNum {requestNum} not implemented in MockServerCommProtocol.");
            }
        }

        private Task<CommData> LoginRequestToken(JsonObject parameters)
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

            return Task.FromResult(new CommData
            {
                Type = CommMessageType.Request,
                Id = (int)NextId,
                RequestNum = RequestNum.LoginRequestToken,
                Params = new JsonObject
                {
                    { "userDbId", user.DbId }
                }
            });
        }

        private Task<CommData> UserDbIdsRequest(JsonObject parameters)
        {
            List<DbId> userDbIds = _mockData.Users.Select(u => u.DbId).ToList();
            return Task.FromResult(new CommData
            {
                Type = CommMessageType.Request,
                Id = (int)NextId,
                RequestNum = RequestNum.UserDbIds,
                Params = new JsonObject
                {
                    { "userDbIds", JsonSerializer.SerializeToNode(userDbIds) }
                }
            });
        }

        private Task<CommData> UserInfoListRequest(JsonObject parameters)
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
                if (!result.ContainsKey("userInfoList"))
                {
                    result["userInfoList"] = new JsonArray();
                }
                ((JsonArray)result["userInfoList"]!).Add(userData);

            }

            return Task.FromResult(new CommData
            {
                Type = CommMessageType.Request,
                Id = (int)NextId,
                RequestNum = RequestNum.UserInfoList,
                Params = result
            });
        }

        private Task<CommData> AccountInfoListRequest(JsonObject parameters)
        {
            var accounts = _mockData.Users.SelectMany(u => u.Accounts).ToList();

            JsonObject result = new JsonObject();
            foreach (var account in accounts)
            {
                var accountData = new JsonObject
                {
                    { "dbId", account.DbId },
                    { "UserDbId", account.User.DbId },
                                    };
                if (!result.ContainsKey("accountInfoList"))
                {
                    result["accountInfoList"] = new JsonArray();
                }
                ((JsonArray)result["accountInfoList"]!).Add(accountData);

            }

            return Task.FromResult(new CommData
            {
                Type = CommMessageType.Request,
                Id = (int)NextId,
                RequestNum = RequestNum.AccountInfoList,
                Params = result
            });
        }

        private Task<CommData> DriveInfoListRequest(JsonObject parameters)
        {
            var drives = _mockData.Users.SelectMany(u => u.Accounts).SelectMany(a => a.Drives).ToList();

            JsonObject result = new JsonObject();
            foreach (var drive in drives)
            {
                var driveData = new JsonObject
                {
                    { "dbId", drive.DbId },
                    { "driveId", drive.DriveId },
                    { "accountDbId", drive.Account.DbId },
                    { "name", Convert.ToBase64String(Encoding.UTF8.GetBytes(drive.Name)) },
                    { "color", Convert.ToBase64String(Encoding.UTF8.GetBytes(System.Drawing.ColorTranslator.ToHtml(drive.Color))) },
                    { "notifications", true },
                    { "maintenance", false },
                    { "locked", false },
                    { "accessDenied", false }
                };
                if (!result.ContainsKey("driveInfoList"))
                {
                    result["driveInfoList"] = new JsonArray();
                }
                ((JsonArray)result["driveInfoList"]!).Add(driveData);
            }

            return Task.FromResult(new CommData
            {
                Type = CommMessageType.Request,
                Id = (int)NextId,
                RequestNum = RequestNum.DriveInfoList,
                Params = result
            });
        }

        private Task<CommData> SyncInfoListRequest(JsonObject parameters)
        {
            var syncs = _mockData.Users.SelectMany(u => u.Accounts).SelectMany(a => a.Drives).SelectMany(d => d.Syncs).ToList();
            JsonObject result = new JsonObject();
            foreach (var sync in syncs)
            {
                var syncData = new JsonObject
                {
                    { "dbId", sync.DbId },
                    { "driveDbId", sync.Drive.DbId },
                    { "localPath", Convert.ToBase64String(Encoding.UTF8.GetBytes(sync.LocalPath)) },
                    { "targetPath", Convert.ToBase64String(Encoding.UTF8.GetBytes(sync.RemotePath)) },
                    { "targetNodeId", "" },
                    { "supportOnlineMode", sync.SupportOnlineMode },
                    { "syncType", (int)sync.SyncType }
                };
                if (!result.ContainsKey("syncInfoList"))
                {
                    result["syncInfoList"] = new JsonArray();
                }
                ((JsonArray)result["syncInfoList"]!).Add(syncData);

            }

            return Task.FromResult(new CommData
            {
                Type = CommMessageType.Request,
                Id = (int)NextId,
                RequestNum = RequestNum.SyncInfoList,
                Params = result
            });
        }

        private async Task<CommData> UpdateStartInstaller(JsonObject parameters)
        {
            Logger.Log(Logger.Level.Debug, "Received UpdateStartInstaller request.");
            await Task.Delay(2000);
            return new CommData
            {
                Type = CommMessageType.Request,
                Id = (int)NextId,
                RequestNum = RequestNum.UPDATER_START_INSTALLER,
                Params = new JsonObject()
            };
        }

        private async Task<CommData> UpdaterVersionInfo(JsonObject parameters)
        {
            Logger.Log(Logger.Level.Debug, "Received UpdateVersionInfo request.");
            VersionChannel channel = _mockData.CurrentChannel;
            if (parameters.ContainsKey("channel") && parameters["channel"] != null)
            {
                channel = (VersionChannel)parameters["channel"]!.GetValue<int>();
            }

            AppVersion update = _mockData.VersionsByChannel[channel] ?? _mockData.CurrentVersion;
            var versionInfo = new JsonObject
            {
                { "channel", (int)update.Channel },
                { "tag", Convert.ToBase64String(Encoding.UTF8.GetBytes(update.Tag)) },
                { "buildVersion", Convert.ToBase64String(Encoding.UTF8.GetBytes(update.BuildVersion)) },
                { "buildMinOsVersion","" }, // Not used
                { "downloadUrl", "" }, // Not used
            };

            return new CommData
            {
                Type = CommMessageType.Request,
                Id = (int)NextId,
                RequestNum = RequestNum.UPDATER_VERSION_INFO,
                Params = new JsonObject() { { "versionInfo", versionInfo } }
            };
        }
        private async Task<CommData> UpdaterChangeChannel(JsonObject parameters)
        {
            Logger.Log(Logger.Level.Debug, "Received UpdaterChangeChannel request.");
            VersionChannel channel = _mockData.CurrentChannel;
            if (parameters.ContainsKey("channel") && parameters["channel"] != null)
            {
                channel = (VersionChannel)parameters["channel"]!.GetValue<int>();
            }
            else
            {
                Logger.Log(Logger.Level.Warning, "No channel specified in UpdaterChangeChannel request, using current channel.");

            }
            _mockData.CurrentChannel = channel;
            EnqueueSignal(SignalNum.UPDATER_STATE_CHANGED, new JsonObject());
            return new CommData
            {
                Type = CommMessageType.Request,
                Id = (int)NextId,
                RequestNum = RequestNum.UPDATER_CHANGE_CHANNEL,
                Params = new JsonObject()
            };
        }

        private async Task<CommData> ParametersInfo(JsonObject parameters)
        {
            Logger.Log(Logger.Level.Debug, "Received ParametersInfo request.");
            var options = new JsonSerializerOptions
            {
                WriteIndented = true,
            };
            string parametersInfo = JsonSerializer.Serialize(_mockData.Settings, options);

            return new CommData
            {
                Type = CommMessageType.Request,
                Id = (int)NextId,
                RequestNum = RequestNum.UPDATER_CHANGE_CHANNEL,
                Params = new JsonObject
                {
                    ["parmsInfo"] = JsonNode.Parse(parametersInfo)
                }
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
            _customSignalsHandler = SimulateSignals();
            return Task.CompletedTask;
        }

        public async Task SimulateSignals()
        {
            long updaterStateChangeCounter = 0;
            while (true)
            {
                updaterStateChangeCounter++;
                await Task.Delay(100);
                if (updaterStateChangeCounter % 50 == 0)
                {
                    if (!_mockData.VersionsByChannel.ContainsKey(VersionChannel.Internal)) continue;
                    string oldTag = _mockData.VersionsByChannel[VersionChannel.Internal]?.Tag ?? "0.0.0";
                    _mockData.VersionsByChannel[VersionChannel.Internal]!.Tag = "3.7.9" + ((updaterStateChangeCounter / 50)).ToString();
                    EnqueueSignal(SignalNum.UPDATER_STATE_CHANGED, new JsonObject());
                }
            }
        }
    }

    public struct MockServerData
    {
        // Save the structure to a JSON file
        public void save(string path)
        {
            var options = new JsonSerializerOptions
            {
                WriteIndented = true,
                ReferenceHandler = System.Text.Json.Serialization.ReferenceHandler.Preserve
            };
            var json = JsonSerializer.Serialize(this, options);
            System.IO.File.WriteAllText(path, json);
        }

        public List<User> Users { get; set; } = new List<User>();
        public AppVersion CurrentVersion { get; set; } = new AppVersion() { BuildVersion = "20250908", Tag = "3.7.6" };
        public ParmsInfo Settings { get; set; }

        public Dictionary<VersionChannel, AppVersion?> VersionsByChannel { get; set; } = new Dictionary<VersionChannel, AppVersion?>()
        {
            {VersionChannel.Prod, new AppVersion() { BuildVersion = "20250908", Tag = "3.7.6" } },
            {VersionChannel.Beta, new AppVersion() { BuildVersion = "20251020", Tag = "3.7.7" } },
            {VersionChannel.Internal, new AppVersion() { BuildVersion = "20251022", Tag = "3.7.8" }},
        };

        public VersionChannel CurrentChannel = VersionChannel.Prod;
        public MockServerData()
        {
            InitializeMockData();
        }

        public void InitializeMockData()
        {
            // Create mock users
            Users.Add(new User(1) { UserId = 10, Name = "John", Email = "John.doe@infomaniak.com", IsConnected = true, IsStaff = false });
            Users.Add(new User(2) { UserId = 11, Name = "Alice", Email = "Alice.doe@infomaniak.com", IsConnected = false, IsStaff = false });

            // Create mock accounts
            List<Account> accounts = new List<Account>();
            accounts.Add(new Account(1, Users[0]));
            accounts.Add(new Account(2, Users[1]));

            Users[0].Accounts.Add(accounts[0]);
            Users[1].Accounts.Add(accounts[1]);

            // Create mock drives
            List<Drive> drives = new List<Drive>();
            drives.Add(new Drive(1, accounts[0]) { DriveId = 140946, Name = "Infomaniak", Color = Color.FromArgb(255, 0, 150, 136), IsPaidOffer = true });
            drives.Add(new Drive(2, accounts[0]) { DriveId = 101, Name = "Etik corp", Color = Color.FromArgb(255, 156, 38, 176), IsPaidOffer = true });
            drives.Add(new Drive(3, accounts[0]) { DriveId = 102, Name = "CH corp", Color = Color.FromArgb(255, 110, 168, 44), IsPaidOffer = false });
            drives.Add(new Drive(4, accounts[0]) { DriveId = 103, Name = "The cloud", Color = Color.FromArgb(255, 255, 168, 110), IsPaidOffer = false });
            drives.Add(new Drive(5, accounts[0]) { DriveId = 104, Name = "SwissCloud", Color = Color.FromArgb(255, 160, 168, 213), IsPaidOffer = false });
            drives.Add(new Drive(6, accounts[0]) { DriveId = 105, Name = "FrenchCloud", Color = Color.FromArgb(255, 123, 179, 12), IsPaidOffer = false });
            drives.Add(new Drive(7, accounts[1]) { DriveId = 106, Name = "EuropaCloud", Color = Color.FromArgb(255, 160, 12, 213), IsPaidOffer = false });
            drives.Add(new Drive(8, accounts[1]) { DriveId = 107, Name = "WinUI cloud", Color = Color.FromArgb(255, 12, 168, 179), IsPaidOffer = false });

            Users[0].Accounts[0].Drives.Add(drives[0]);
            Users[0].Accounts[0].Drives.Add(drives[1]);
            Users[0].Accounts[0].Drives.Add(drives[2]);
            Users[0].Accounts[0].Drives.Add(drives[3]);
            Users[0].Accounts[0].Drives.Add(drives[4]);
            Users[0].Accounts[0].Drives.Add(drives[5]);
            Users[1].Accounts[0].Drives.Add(drives[6]);
            Users[1].Accounts[0].Drives.Add(drives[7]);


            // Create mock syncs
            List<Sync> syncs = new List<Sync>();

            syncs.Add(new Sync(1, drives[0]) { Id = 1000, LocalPath = "C:\\Users\\John\\Etik corp sync1", RemotePath = "", SupportOnlineMode = false });
            drives[0].Syncs.Add(syncs[0]);

            syncs.Add(new Sync(2, drives[1]) { Id = 1001, LocalPath = "D:\\Users\\John\\CH corp\\kDrive Metier", RemotePath = "", SupportOnlineMode = false });
            drives[1].Syncs.Add(syncs[1]);

            syncs.Add(new Sync(3, drives[2]) { Id = 1002, LocalPath = "F:\\Users\\John\\CH corp\\kDrive Adminstration", RemotePath = "", SupportOnlineMode = false });
            syncs.Add(new Sync(4, drives[2]) { Id = 1003, LocalPath = "F:\\Users\\John\\Music", RemotePath = "", SupportOnlineMode = false });
            drives[2].Syncs.Add(syncs[2]);
            drives[2].Syncs.Add(syncs[3]);

            syncs.Add(new Sync(5, drives[3]) { Id = 1004, LocalPath = "F:\\Users\\John\\Photos", RemotePath = "", SupportOnlineMode = false });
            syncs.Add(new Sync(6, drives[3]) { Id = 1005, LocalPath = "F:\\Users\\John\\Famille\\Photos", RemotePath = "", SupportOnlineMode = false });
            syncs.Add(new Sync(7, drives[3]) { Id = 1006, LocalPath = "F:\\Users\\John\\vidéo", RemotePath = "", SupportOnlineMode = false });
            drives[3].Syncs.Add(syncs[4]);
            drives[3].Syncs.Add(syncs[5]);
            drives[3].Syncs.Add(syncs[6]);


            syncs.Add(new Sync(8, drives[4]) { Id = 1007, LocalPath = "F:\\Users\\John\\Film", RemotePath = "", SupportOnlineMode = false });
            syncs.Add(new Sync(9, drives[5]) { Id = 1008, LocalPath = "F:\\Users\\John\\Pro\\Comptabilité", RemotePath = "", SupportOnlineMode = false });
            syncs.Add(new Sync(10, drives[6]) { Id = 1009, LocalPath = "F:\\Users\\John\\Pro\\Rh", RemotePath = "", SupportOnlineMode = false });
            syncs.Add(new Sync(11, drives[7]) { Id = 1010, LocalPath = "F:\\Users\\John\\The cloud sync8", RemotePath = "", SupportOnlineMode = false });
            drives[4].Syncs.Add(syncs[7]);
            drives[5].Syncs.Add(syncs[8]);
            drives[6].Syncs.Add(syncs[9]);
            drives[7].Syncs.Add(syncs[10]);


            syncs.Add(new Sync(13, drives[4]) { Id = 1013, LocalPath = "F:\\Users\\John\\SwissCloud 1", RemotePath = "", SupportOnlineMode = false });
            syncs.Add(new Sync(14, drives[4]) { Id = 1014, LocalPath = "F:\\Users\\John\\SwissCloud 2", RemotePath = "", SupportOnlineMode = false });
            syncs.Add(new Sync(15, drives[4]) { Id = 1015, LocalPath = "F:\\Users\\John\\SwissCloud 3", RemotePath = "", SupportOnlineMode = false });
            syncs.Add(new Sync(16, drives[4]) { Id = 1016, LocalPath = "F:\\Users\\John\\SwissCloud 4", RemotePath = "", SupportOnlineMode = false });
            syncs.Add(new Sync(17, drives[4]) { Id = 1017, LocalPath = "F:\\Users\\John\\SwissCloud 5", RemotePath = "", SupportOnlineMode = false });
            syncs.Add(new Sync(18, drives[4]) { Id = 1018, LocalPath = "F:\\Users\\John\\SwissCloud 6", RemotePath = "", SupportOnlineMode = false });
            syncs.Add(new Sync(19, drives[4]) { Id = 1018, LocalPath = "F:\\Users\\John\\SwissCloud 7", RemotePath = "", SupportOnlineMode = false });
            drives[4].Syncs.Add(syncs[11]);
            drives[4].Syncs.Add(syncs[12]);
            drives[4].Syncs.Add(syncs[13]);
            drives[4].Syncs.Add(syncs[14]);
            drives[4].Syncs.Add(syncs[15]);
            drives[4].Syncs.Add(syncs[16]);
            drives[4].Syncs.Add(syncs[17]);

            // Create mock ParmsInfo
            ParmsInfo parmsInfo = new ParmsInfo()
            {
                Language = Language.SystemDefault,
                AutoStart = true,
                MoveToTrash = true,
                NotificationsDisabled = NotificationsDisabled.Never,
                UseLog = false,
                LogLevel = Logger.Level.Debug,
                ExtendedLog = false,
                PurgeOldLogs = true,
                ShowShortcuts = true,
                ProxyConfigInfo = new ProxyConfigInfo()
                {
                    Type = ProxyType.None,
                    HostName = "",
                    Port = 0,
                    NeedsAuth = false,
                    User = "",
                    Pwd = ""
                }
            };

            Settings = parmsInfo;
        }
    }
}