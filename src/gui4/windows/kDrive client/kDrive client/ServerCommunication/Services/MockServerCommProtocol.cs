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

using Infomaniak.kDrive.ServerCommunication.CommStruct;
using Infomaniak.kDrive.ServerCommunication.JsonConverters;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;
using static Infomaniak.kDrive.ServerCommunication.Interfaces.IServerCommProtocol;

namespace Infomaniak.kDrive.ServerCommunication.Services
{
    public class MockServerCommProtocol : Interfaces.IServerCommProtocol
    {
        protected MockServerData _mockData = new();

        private long _requestIdCounter = 0;
        private long NextId
        {
            get => ++_requestIdCounter;
        }

        public Task<bool> InitConnection(CancellationToken cancellationToken) { return Task.FromResult(true); }
        public event EventHandler<SignalEventArgs>? SignalReceived;

#pragma warning disable CS0067 // ConnectionLost is not used in the mock implementation, but we need to declare it to satisfy the IServerCommProtocol interface. We can safely ignore the fact that it's never raised.
        public event EventHandler? ConnectionLost;
#pragma warning restore CS0067 

        protected Queue<KeyValuePair<SignalNum, JsonObject>> PendingSignals { get; } = new Queue<KeyValuePair<SignalNum, JsonObject>>();
        private Task? _signalHandler;
        private Task? _customSignalsHandler;
        public MockServerCommProtocol()
        {
            Initialize();
        }

        public async Task<CommData> SendRequestAsync(RequestNum requestNum, JsonObject parameters, CancellationToken cancellationToken = default)
        {
            return requestNum switch
            {
                RequestNum.LOGIN_REQUESTTOKEN => await LoginRequestToken(parameters),
                RequestNum.USER_DBIDLIST => await UserDbIdsRequest(parameters),
                RequestNum.USER_INFOLIST => await UserInfoListRequest(parameters),
                RequestNum.ACCOUNT_INFOLIST => await AccountInfoListRequest(parameters),
                RequestNum.DRIVE_INFOLIST => await DriveInfoListRequest(parameters),
                RequestNum.SYNC_INFOLIST => await SyncInfoListRequest(parameters),
                RequestNum.UPDATER_START_INSTALLER => await UpdateStartInstaller(parameters),
                RequestNum.UPDATER_SKIP_VERSION => await UpdaterSkipVersion(parameters),
                RequestNum.UPDATER_VERSION_INFO => await UpdaterVersionInfo(parameters),
                RequestNum.UPDATER_CHANGE_CHANNEL => await UpdaterChangeChannel(parameters),
                RequestNum.PARAMETERS_INFO => await ParametersInfo(parameters),
                RequestNum.PARAMETERS_UPDATE => await ParametersUpdate(parameters),
                RequestNum.ERROR_DELETE => await ErrorDelete(parameters),
                _ => throw new NotImplementedException($"RequestNum {requestNum} not implemented in MockServerCommProtocol.")
            };
        }

        private Task<CommData> LoginRequestToken(JsonObject parameters)
        {
            var random = new Random();
            var randomUserIndex = random.Next(0, _mockData.Users.Count);
            var user = _mockData.Users.ElementAtOrDefault(randomUserIndex) ?? throw new InvalidOperationException("No users available in mock data.");

            // Simulate UserAdded signal
            var userData = new JsonObject
            {
                { JsonKeys.UserInfo, new JsonObject
                {
                    { "userDbId", user.DbId },
                    { "userId", user.UserId },
                    { "name", user.Name },
                    { "email", user.Email },
                    { "isConnected", user.IsConnected },
                    { "isStaff", user.IsStaff }
                }}
            };
            EnqueueSignal(SignalNum.USER_ADDED, userData);

            return Task.FromResult(new CommData
            {
                Type = CommMessageType.Request,
                Id = (int)NextId,
                RequestNum = RequestNum.LOGIN_REQUESTTOKEN,
                Params = new JsonObject
                {
                    { "userDbId", user.DbId }
                }
            });
        }

        private Task<CommData> UserDbIdsRequest(JsonObject parameters)
        {
            List<DbId> userDbIds = [.. _mockData.Users.Select(u => u.DbId)];
            return Task.FromResult(new CommData
            {
                Type = CommMessageType.Request,
                Id = (int)NextId,
                RequestNum = RequestNum.USER_DBIDLIST,
                Params = new JsonObject
                {
                    { JsonKeys.UserDbIds, JsonSerializer.SerializeToNode(userDbIds) }
                }
            });
        }

        private Task<CommData> UserInfoListRequest(JsonObject parameters)
        {
            var users = _mockData.Users;

            JsonObject result = [];
            foreach (var user in users)
            {
                var userData = new JsonObject
                {
                    { "dbId", user.DbId },
                    { "userId", user.UserId },
                    { "name", Utility.ToBase64String(user.Name) },
                    { "email", Utility.ToBase64String(user.Email) },
                    { "isConnected", user.IsConnected },
                    { "isStaff", user.IsStaff }
                };
                if (!result.ContainsKey(JsonKeys.UserInfoList))
                {
                    result[JsonKeys.UserInfoList] = new JsonArray();
                }
                ((JsonArray)result[JsonKeys.UserInfoList]!).Add(userData);

            }

            return Task.FromResult(new CommData
            {
                Type = CommMessageType.Request,
                Id = (int)NextId,
                RequestNum = RequestNum.USER_INFOLIST,
                Params = result
            });
        }

        private Task<CommData> AccountInfoListRequest(JsonObject parameters)
        {
            var accounts = _mockData.Users.SelectMany(u => u.Accounts).ToList();

            JsonObject result = [];
            foreach (var account in accounts)
            {
                var accountData = new JsonObject
                {
                    { "dbId", account.DbId },
                    { JsonKeys.UserDbId, account.User.DbId },
                    { JsonKeys.AccountName, account.Name },
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
                RequestNum = RequestNum.ACCOUNT_INFOLIST,
                Params = result
            });
        }

        private Task<CommData> DriveInfoListRequest(JsonObject parameters)
        {
            var drives = _mockData.Users.SelectMany(u => u.Accounts).SelectMany(a => a.Drives).ToList();

            JsonObject result = [];
            foreach (var drive in drives)
            {
                var driveData = new JsonObject
                {
                    { "dbId", drive.DbId },
                    { "driveId", drive.DriveId },
                    { "accountDbId", drive.Account.DbId },
                    { "name", Utility.ToBase64String(drive.Name) },
                    { "color", Utility.ToBase64String(System.Drawing.ColorTranslator.ToHtml(drive.Color)) },
                    { "notifications", true },
                    { "maintenance", false },
                    { "locked", false },
                    { "accessDenied", false },
                    { "size", 0 },
                    { "usedSize", 0 }
                };
                if (!result.ContainsKey(JsonKeys.DriveInfoList))
                {
                    result[JsonKeys.DriveInfoList] = new JsonArray();
                }
                ((JsonArray)result[JsonKeys.DriveInfoList]!).Add(driveData);
            }

            return Task.FromResult(new CommData
            {
                Type = CommMessageType.Request,
                Id = (int)NextId,
                RequestNum = RequestNum.DRIVE_INFOLIST,
                Params = result
            });
        }

        private Task<CommData> SyncInfoListRequest(JsonObject parameters)
        {
            var syncs = _mockData.Users.SelectMany(u => u.Accounts).SelectMany(a => a.Drives).SelectMany(d => d.Syncs).ToList();
            JsonObject result = [];
            foreach (var sync in syncs)
            {
                var syncData = new JsonObject
                {
                    { "dbId", sync.DbId },
                    { "driveDbId", sync.Drive.DbId },
                    { "localPath", Utility.ToBase64String(sync.LocalPath) },
                    { "targetPath", Utility.ToBase64String(sync.RemotePath) },
                    { "targetNodeId", "" },
                    { "supportOnlineMode", sync.SupportOnlineMode },
                    { "syncType", (int)sync.SyncType }
                };
                if (!result.ContainsKey(JsonKeys.SyncInfoList))
                {
                    result[JsonKeys.SyncInfoList] = new JsonArray();
                }
                ((JsonArray)result[JsonKeys.SyncInfoList]!).Add(syncData);

            }

            return Task.FromResult(new CommData
            {
                Type = CommMessageType.Request,
                Id = (int)NextId,
                RequestNum = RequestNum.SYNC_INFOLIST,
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

        private async Task<CommData> UpdaterSkipVersion(JsonObject parameters)
        {
            Logger.Log(Logger.Level.Debug, "Received UpdaterSkipVersion request.");
            await Task.CompletedTask;
            return new CommData
            {
                Type = CommMessageType.Request,
                Id = (int)NextId,
                RequestNum = RequestNum.UPDATER_SKIP_VERSION,
                Params = new JsonObject()
            };
        }

        private async Task<CommData> UpdaterVersionInfo(JsonObject parameters)
        {
            Logger.Log(Logger.Level.Debug, "Received UpdateVersionInfo request.");
            DistributionChannel channel = _mockData.Settings.DistributionChannel ?? throw new InvalidOperationException("Distribution channel is not set in mocked settings.");
            if (parameters.ContainsKey(JsonKeys.UpdateChannel) && parameters[JsonKeys.UpdateChannel] != null)
            {
                channel = (DistributionChannel)parameters[JsonKeys.UpdateChannel]!.GetValue<int>();
            }

            AppVersion update = _mockData.VersionsByChannel[channel] ?? _mockData.CurrentVersion;
            var versionInfo = new JsonObject
            {
                { "channel", (int)update.Channel },
                { "tag", Utility.ToBase64String(update.Tag) },
                { "buildVersion", update.BuildVersion },
                { "buildMinOsVersion","" }, // Not used
                { "downloadUrl", "" }, // Not used
            };

            return new CommData
            {
                Type = CommMessageType.Request,
                Id = (int)NextId,
                RequestNum = RequestNum.UPDATER_VERSION_INFO,
                Params = new JsonObject() { { JsonKeys.VersionInfo, versionInfo } }
            };
        }
        private async Task<CommData> UpdaterChangeChannel(JsonObject parameters)
        {
            Logger.Log(Logger.Level.Debug, "Received UpdaterChangeChannel request.");
            if (parameters.ContainsKey(JsonKeys.UpdateChannel) && parameters[JsonKeys.UpdateChannel] != null)
            {
                _mockData.Settings.DistributionChannel = (DistributionChannel)parameters[JsonKeys.UpdateChannel]!.GetValue<int>();
            }
            else
            {
                Logger.Log(Logger.Level.Warning, "No channel specified in UpdaterChangeChannel request, using current channel.");

            }
            EnqueueSignal(SignalNum.UPDATER_STATE_CHANGED, []);
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
                    [JsonKeys.ParmsInfo] = JsonNode.Parse(parametersInfo)
                }
            };
        }

        private async Task<CommData> ParametersUpdate(JsonObject parameters)
        {
            Logger.Log(Logger.Level.Debug, "Received ParametersUpdate request.");
            if (!parameters.ContainsKey(JsonKeys.ParmsInfo) || parameters[JsonKeys.ParmsInfo] == null)
            {
                Logger.Log(Logger.Level.Warning, "No parmsInfo specified in ParametersUpdate request, using existing settings.");

                return new CommData
                {
                    Type = CommMessageType.Request,
                    Id = (int)NextId,
                    RequestNum = RequestNum.PARAMETERS_UPDATE,
                    Params = new JsonObject()
                };
            }

            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            var updatedSettings = parameters[JsonKeys.ParmsInfo]!.Deserialize<ParmsInfo>(options);
            if (updatedSettings is null)
            {
                Logger.Log(Logger.Level.Error, $"Failed to deserialize parmsInfo from ${parameters["parmsInfos"]}");
                throw new InvalidOperationException("Failed to deserialize parmsInfo in ParametersUpdate request.");
            }
            _mockData.Settings = updatedSettings;

            return new CommData
            {
                Type = CommMessageType.Request,
                Id = (int)NextId,
                RequestNum = RequestNum.PARAMETERS_UPDATE,
                Params = new JsonObject()
            };
        }

        private Task<CommData> ErrorDelete(JsonObject parameters)
        {
            DbId errorDbId = parameters[JsonKeys.ErrorDbId]?.GetValue<DbId>() ?? -1;
            EnqueueSignal(SignalNum.UTILITY_ERROR_REMOVED, new JsonObject { [JsonKeys.ErrorDbId] = errorDbId });
            return Task.FromResult(new CommData
            {
                Type = CommMessageType.Request,
                Id = (int)NextId,
                RequestNum = RequestNum.ERROR_DELETE,
                Params = new JsonObject()
            });
        }

        protected void EnqueueSignal(SignalNum signalNum, JsonObject parameters)
        {
            PendingSignals.Enqueue(new KeyValuePair<SignalNum, JsonObject>(signalNum, parameters));
        }

        protected void RaiseSignal(SignalNum signalNum, JsonObject parameters)
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

        public virtual async Task SimulateSignals()
        {
        }
    }

    public struct MockServerData
    {
        // Save the structure to a JSON file
        public void Save(string path)
        {
            var options = new JsonSerializerOptions
            {
                WriteIndented = true,
                ReferenceHandler = System.Text.Json.Serialization.ReferenceHandler.Preserve
            };
            var json = JsonSerializer.Serialize(this, options);
            System.IO.File.WriteAllText(path, json);
        }

        public List<User> Users { get; set; } = [];
        public AppVersion CurrentVersion { get; set; } = new AppVersion() { BuildVersion = 1, Tag = "3.7.6" };
        public ParmsInfo Settings { get; set; } = new ParmsInfo();

        public Dictionary<DistributionChannel, AppVersion?> VersionsByChannel { get; set; } = new Dictionary<DistributionChannel, AppVersion?>()
        {
            {DistributionChannel.Prod, new AppVersion() { BuildVersion = 1, Tag = "3.7.6" } },
            {DistributionChannel.Beta, new AppVersion() { BuildVersion = 2, Tag = "3.7.7" } },
            {DistributionChannel.Internal, new AppVersion() { BuildVersion = 2, Tag = "3.7.8" }},
        };

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
            List<Account> accounts = [new Account(1, Users[0]), new Account(2, Users[1])];

            Users[0].Accounts.Add(accounts[0]);
            Users[1].Accounts.Add(accounts[1]);

            // Create mock drives
            List<Drive> drives =
            [
                new Drive(1, accounts[0]) { DriveId = 140946, Name = "Infomaniak", Color = Color.FromArgb(255, 0, 150, 136), IsFreeOffer = false },
                new Drive(2, accounts[0]) { DriveId = 101, Name = "Etik corp", Color = Color.FromArgb(255, 156, 38, 176), IsFreeOffer = false },
                new Drive(3, accounts[0]) { DriveId = 102, Name = "CH corp", Color = Color.FromArgb(255, 110, 168, 44), IsFreeOffer = true },
                new Drive(4, accounts[0]) { DriveId = 103, Name = "The cloud", Color = Color.FromArgb(255, 255, 168, 110), IsFreeOffer = true },
                new Drive(5, accounts[0]) { DriveId = 104, Name = "SwissCloud", Color = Color.FromArgb(255, 160, 168, 213), IsFreeOffer = true },
                new Drive(6, accounts[0]) { DriveId = 105, Name = "FrenchCloud", Color = Color.FromArgb(255, 123, 179, 12), IsFreeOffer = true },
                new Drive(7, accounts[1]) { DriveId = 106, Name = "EuropaCloud", Color = Color.FromArgb(255, 160, 12, 213), IsFreeOffer = true },
                new Drive(8, accounts[1]) { DriveId = 107, Name = "WinUI cloud", Color = Color.FromArgb(255, 12, 168, 179), IsFreeOffer = true },
            ];

            Users[0].Accounts[0].Drives.Add(drives[0]);
            Users[0].Accounts[0].Drives.Add(drives[1]);
            Users[0].Accounts[0].Drives.Add(drives[2]);
            Users[0].Accounts[0].Drives.Add(drives[3]);
            Users[0].Accounts[0].Drives.Add(drives[4]);
            Users[0].Accounts[0].Drives.Add(drives[5]);
            Users[1].Accounts[0].Drives.Add(drives[6]);
            Users[1].Accounts[0].Drives.Add(drives[7]);


            // Create mock syncs
            List<Sync> syncs =
            [
                new Sync(1, drives[0]) { LocalPath = "C:\\Users\\John\\Etik corp sync1", RemotePath = "", SupportOnlineMode = false },
            ];
            drives[0].Syncs.Add(syncs[0]);

            syncs.Add(new Sync(2, drives[1]) { LocalPath = "D:\\Users\\John\\CH corp\\kDrive Metier", RemotePath = "", SupportOnlineMode = false });
            drives[1].Syncs.Add(syncs[1]);

            syncs.Add(new Sync(3, drives[2]) { LocalPath = "F:\\Users\\John\\CH corp\\kDrive Adminstration", RemotePath = "", SupportOnlineMode = false });
            syncs.Add(new Sync(4, drives[2]) { LocalPath = "F:\\Users\\John\\Music", RemotePath = "", SupportOnlineMode = false });
            drives[2].Syncs.Add(syncs[2]);
            drives[2].Syncs.Add(syncs[3]);

            syncs.Add(new Sync(5, drives[3]) { LocalPath = "F:\\Users\\John\\Photos", RemotePath = "", SupportOnlineMode = false });
            syncs.Add(new Sync(6, drives[3]) { LocalPath = "F:\\Users\\John\\Famille\\Photos", RemotePath = "", SupportOnlineMode = false });
            syncs.Add(new Sync(7, drives[3]) { LocalPath = "F:\\Users\\John\\vidéo", RemotePath = "", SupportOnlineMode = false });
            drives[3].Syncs.Add(syncs[4]);
            drives[3].Syncs.Add(syncs[5]);
            drives[3].Syncs.Add(syncs[6]);


            syncs.Add(new Sync(8, drives[4]) { LocalPath = "F:\\Users\\John\\Film", RemotePath = "", SupportOnlineMode = false });
            syncs.Add(new Sync(9, drives[5]) { LocalPath = "F:\\Users\\John\\Pro\\Comptabilité", RemotePath = "", SupportOnlineMode = false });
            syncs.Add(new Sync(10, drives[6]) { LocalPath = "F:\\Users\\John\\Pro\\Rh", RemotePath = "", SupportOnlineMode = false });
            syncs.Add(new Sync(11, drives[7]) { LocalPath = "F:\\Users\\John\\The cloud sync8", RemotePath = "", SupportOnlineMode = false });
            drives[4].Syncs.Add(syncs[7]);
            drives[5].Syncs.Add(syncs[8]);
            drives[6].Syncs.Add(syncs[9]);
            drives[7].Syncs.Add(syncs[10]);


            syncs.Add(new Sync(13, drives[4]) { LocalPath = "F:\\Users\\John\\SwissCloud 1", RemotePath = "", SupportOnlineMode = false });
            syncs.Add(new Sync(14, drives[4]) { LocalPath = "F:\\Users\\John\\SwissCloud 2", RemotePath = "", SupportOnlineMode = false });
            syncs.Add(new Sync(15, drives[4]) { LocalPath = "F:\\Users\\John\\SwissCloud 3", RemotePath = "", SupportOnlineMode = false });
            syncs.Add(new Sync(16, drives[4]) { LocalPath = "F:\\Users\\John\\SwissCloud 4", RemotePath = "", SupportOnlineMode = false });
            syncs.Add(new Sync(17, drives[4]) { LocalPath = "F:\\Users\\John\\SwissCloud 5", RemotePath = "", SupportOnlineMode = false });
            syncs.Add(new Sync(18, drives[4]) { LocalPath = "F:\\Users\\John\\SwissCloud 6", RemotePath = "", SupportOnlineMode = false });
            syncs.Add(new Sync(19, drives[4]) { LocalPath = "F:\\Users\\John\\SwissCloud 7", RemotePath = "", SupportOnlineMode = false });
            drives[4].Syncs.Add(syncs[11]);
            drives[4].Syncs.Add(syncs[12]);
            drives[4].Syncs.Add(syncs[13]);
            drives[4].Syncs.Add(syncs[14]);
            drives[4].Syncs.Add(syncs[15]);
            drives[4].Syncs.Add(syncs[16]);
            drives[4].Syncs.Add(syncs[17]);

            // Create mock ParmsInfo
            ParmsInfo parmsInfo = new()
            {
                Language = Language.Default,
                AutoStart = true,
                MoveToTrash = true,
                NotificationsDisabled = NotificationsDisabled.Always,
                DistributionChannel = DistributionChannel.Prod,
                UseLog = false,
                LogLevel = Logger.Level.Debug,
                ExtendedLog = false,
                PurgeOldLogs = true,
                ProxyConfigInfo = new ProxyConfigInfo()
                {
                    Type = ProxyType.HTTP,
                    HostName = "192.168.1.54",
                    Port = 8080,
                    NeedsAuth = true,
                    User = "Alice",
                    Pwd = "dds"
                }
            };

            Settings = parmsInfo;
        }
    }
}