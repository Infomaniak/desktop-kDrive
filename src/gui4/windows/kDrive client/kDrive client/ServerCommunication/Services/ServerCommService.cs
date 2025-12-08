using Infomaniak.kDrive.ServerCommunication.CommStruct;
using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.ServerCommunication.JsonConverters;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;
using static Infomaniak.kDrive.ServerCommunication.Interfaces.IServerCommProtocol;

namespace Infomaniak.kDrive.ServerCommunication.Services
{
    public class ServerCommService : IServerCommService
    {
        private readonly IServerCommProtocol _commClient;
        private readonly AppModel _viewModel;

        public ServerCommService(IServerCommProtocol commClient, AppModel viewModel)
        {
            _commClient = commClient;
            _viewModel = viewModel;
            _commClient.SignalReceived += OnSignalReceived;
        }

        // Requests
        public async Task<List<DbId>?> GetUserDbIds(CancellationToken cancellationToken)
        {
            CommData data = await _commClient.SendRequestAsync(RequestNum.USER_DBIDLIST, new JsonObject(), cancellationToken);
            if (data.Params == null || !data.Params.ContainsKey(JsonKeys.UserDbIds))
            {
                Logger.Log(Logger.Level.Error, $"{JsonKeys.UserDbIds} not found in response.");
                return null;
            }
            return data.Params[JsonKeys.UserDbIds].Deserialize<List<DbId>>();
        }

        public async Task<User?> AddOrRelogUser(string oAuthCode, string oAuthCodeVerifier, CancellationToken cancellationToken)
        {
            CommData data = await _commClient.SendRequestAsync(RequestNum.LOGIN_REQUESTTOKEN, new JsonObject
            {
                [JsonKeys.OAuthCode] = Utility.ToBase64String(oAuthCode),
                [JsonKeys.OAuthCodeVerifier] = Utility.ToBase64String(oAuthCodeVerifier)
            }, cancellationToken);

            if (data.Params == null || !data.Params.ContainsKey(JsonKeys.UserDbId) || data.Params[JsonKeys.UserDbId] == null)
            {
                Logger.Log(Logger.Level.Error, $"{JsonKeys.UserDbId} not found in response.");
                return null;
            }

            DbId userDbId = data.Params[JsonKeys.UserDbId]?.GetValue<DbId>() ?? -1;

            await Utility.RunOnUIThread(() =>
            {
                if (_viewModel.Users.Any(u => u.DbId == userDbId))
                {
                    Logger.Log(Logger.Level.Info, $"User with DbId {userDbId} already exists in the application.");
                }
                else
                {
                    User newUser = new User(userDbId);
                    _viewModel.Users.Add(newUser);
                    Logger.Log(Logger.Level.Info, $"AddOrRelogUser: New user added with DbId {userDbId}.");
                }
            });
            return _viewModel.Users.FirstOrDefault(u => u?.DbId == userDbId, null);
        }

        public async Task RefreshUsers(CancellationToken cancellationToken)
        {
            CommData data = await _commClient.SendRequestAsync(RequestNum.USER_INFOLIST, new JsonObject(), cancellationToken);
            if (data.Params == null || !data.Params.ContainsKey(JsonKeys.UserInfoList))
            {
                Logger.Log(Logger.Level.Error, $"{JsonKeys.UserInfoList} not found in response.");
                return;
            }
            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            List<UserInfo> userInfos = data.Params[JsonKeys.UserInfoList].Deserialize<List<UserInfo>>(options) ?? new List<UserInfo>();

            // Add/update users
            foreach (var userInfo in userInfos)
            {
                if (userInfo.DbId is null)
                {
                    Logger.Log(Logger.Level.Error, "userInfo.DbId is null.");
                    continue;
                }
                await AddOrUpdateUserInModel(userInfo);
            }

            // Remove users that are no longer present
            var userDbIds = userInfos.Where(u => u.DbId != null).Select(u => u.DbId).ToHashSet();
            await Utility.RunOnUIThread(() =>
            {
                var usersToRemove = _viewModel.Users.Where(u => !userDbIds.Contains(u.DbId)).ToList();
                foreach (var user in usersToRemove)
                {
                    _viewModel.Users.Remove(user);
                    Logger.Log(Logger.Level.Info, $"User with DbId {user.DbId} removed from the application.");
                }
            });
        }

        public async Task RemoveUser(DbId userDbId, CancellationToken cancellationToken)
        {
            JsonObject parms = new()
            {
                [JsonKeys.UserDbId] = userDbId
            };
            var commData = await _commClient.SendRequestAsync(RequestNum.USER_DELETE, parms, cancellationToken);
        }

        public async Task RefreshAccounts(CancellationToken cancellationToken)
        {
            CommData data = await _commClient.SendRequestAsync(RequestNum.ACCOUNT_INFOLIST, new JsonObject(), cancellationToken);
            if (data.Params == null || !data.Params.ContainsKey(JsonKeys.AccountInfoList))
            {
                Logger.Log(Logger.Level.Error, $"{JsonKeys.AccountInfoList} not found in response.");
                return;
            }
            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            List<AccountInfo> accountInfos = data.Params[JsonKeys.AccountInfoList].Deserialize<List<AccountInfo>>(options) ?? new List<AccountInfo>();

            // Add/update accounts
            foreach (var accountInfo in accountInfos)
            {
                if (accountInfo.DbId is null)
                {
                    Logger.Log(Logger.Level.Error, "accountInfo.DbId is null.");
                    continue;
                }
                await AddOrUpdateAccountInModel(accountInfo);
            }

            // Remove accounts that are no longer present
            var accountDbIds = accountInfos.Where(a => a.DbId != null).Select(a => a.DbId).ToHashSet();

            // Find accounts to remove in all users
            var accountsToRemove = new List<Account>();
            foreach (var user in _viewModel.Users)
            {
                var userAccountsToRemove = user.Accounts.Where(a => !accountDbIds.Contains(a.DbId)).ToList();
                accountsToRemove.AddRange(userAccountsToRemove);
            }

            await Utility.RunOnUIThread(() =>
            {
                foreach (var account in accountsToRemove)
                {
                    var parentUser = account.User;
                    if (parentUser != null)
                    {
                        parentUser.Accounts.Remove(account);
                        Logger.Log(Logger.Level.Info, $"Account with DbId {account.DbId} removed from user DbId {parentUser.DbId}.");
                    }
                }
            });
        }

        public async Task RefreshDrives(CancellationToken cancellationToken)
        {
            CommData data = await _commClient.SendRequestAsync(RequestNum.DRIVE_INFOLIST, new JsonObject(), cancellationToken);
            if (data.Params == null || !data.Params.ContainsKey(JsonKeys.DriveInfoList))
            {
                Logger.Log(Logger.Level.Error, $"{JsonKeys.DriveInfoList} not found in response.");
                return;
            }
            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            options.Converters.Add(new ColorJsonConverter());
            List<DriveInfo> driveInfos = data.Params[JsonKeys.DriveInfoList].Deserialize<List<DriveInfo>>(options) ?? new List<DriveInfo>();

            // Add/update drives
            foreach (var driveInfo in driveInfos)
            {
                if (driveInfo.DbId is null)
                {
                    Logger.Log(Logger.Level.Error, "driveInfo.DbId is null.");
                    continue;
                }
                await AddOrUpdateDriveInModel(driveInfo);
            }

            // Remove drives that are no longer present
            var driveDbIds = driveInfos.Where(d => d.DbId != null).Select(d => d.DbId).ToHashSet();

            // Find drives to remove in all users
            var drivesToRemove = new List<Drive>();
            foreach (var user in _viewModel.Users)
            {
                var userDrivesToRemove = user.Drives.Where(d => !driveDbIds.Contains(d.DbId)).ToList();
                drivesToRemove.AddRange(userDrivesToRemove);
            }

            await Utility.RunOnUIThread(() =>
            {
                foreach (var drive in drivesToRemove)
                {
                    var parentAccount = drive.Account;
                    if (parentAccount != null)
                    {
                        parentAccount.Drives.Remove(drive);
                        Logger.Log(Logger.Level.Info, $"Drive with DbId {drive.DbId} removed from account DbId {parentAccount.DbId}.");
                    }
                }
            });
        }

        public async Task RefreshUserDrivesAvailable(DbId userDbId, CancellationToken cancellationToken)
        {
            JsonObject parms = new()
            {
                [JsonKeys.UserDbId] = userDbId
            };
            CommData data = await _commClient.SendRequestAsync(RequestNum.USER_AVAILABLEDRIVES, parms, cancellationToken);
            if (data.Params == null || !data.Params.ContainsKey(JsonKeys.DriveAvailableInfoList))
            {
                Logger.Log(Logger.Level.Error, $"{JsonKeys.DriveAvailableInfoList} not found in response.");
                return;
            }
            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            options.Converters.Add(new ColorJsonConverter());
            List<DriveAvailableInfo> driveAvailableInfoList = data.Params[JsonKeys.DriveAvailableInfoList].Deserialize<List<DriveAvailableInfo>>(options) ?? new List<DriveAvailableInfo>();
            User? user = _viewModel.Users.FirstOrDefault<User>(u => u.DbId == userDbId);
            if (user is null)
            {
                Logger.Log(Logger.Level.Error, $"User not found for DriveAvailable with dbID {userDbId}.");
                return;
            }

            // We don't clear and re-add all items to avoid UI flickering
            // Build sets for fast lookup
            var newIds = driveAvailableInfoList.Select(d => d.DriveId).ToHashSet();
            var existingIds = user.DrivesAvailable.Select(d => d.DriveId).ToHashSet();

            // Add new drives
            foreach (var info in driveAvailableInfoList)
            {
                if (!existingIds.Contains(info.DriveId ?? -1))
                {
                    var drive = new DriveAvailable();
                    CommStruct.ConversionHelper.copyToDriveAvailable(info, drive);
                    await Utility.RunOnUIThread(() => { user.DrivesAvailable.Add(drive); });
                }
                else
                {
                    // Check if any of the properties have changed, if yes remove and re-add to trigger UI update
                    var existingDrive = user.DrivesAvailable.FirstOrDefault(d => d.DriveId == info.DriveId);
                    if (existingDrive != null)
                    {
                        var tempDrive = new DriveAvailable();
                        CommStruct.ConversionHelper.copyToDriveAvailable(info, tempDrive);
                        // compare properties individually
                        foreach (var prop in typeof(DriveAvailable).GetProperties())
                        {
                            var existingValue = prop.GetValue(existingDrive);
                            var newValue = prop.GetValue(tempDrive);
                            if (!Equals(existingValue, newValue))
                            {
                                await Utility.RunOnUIThread(() =>
                                {
                                    user.DrivesAvailable.Remove(existingDrive);
                                    user.DrivesAvailable.Add(tempDrive);
                                });
                                break;
                            }
                        }
                    }
                }
            }


            // Remove drives no longer available
            foreach (var existingId in existingIds)
            {
                if (!newIds.Contains(existingId))
                {
                    var driveToRemove = user.DrivesAvailable.FirstOrDefault(d => d.DriveId == existingId);
                    if (driveToRemove != null)
                    {
                        await Utility.RunOnUIThread(() => { user.DrivesAvailable.Remove(driveToRemove); });
                    }
                }
            }

        }

        public async Task RefreshSyncs(CancellationToken cancellationToken)
        {
            CommData data = await _commClient.SendRequestAsync(RequestNum.SYNC_INFOLIST, new JsonObject(), cancellationToken);
            if (data.Params == null || !data.Params.ContainsKey(JsonKeys.SyncInfoList))
            {
                Logger.Log(Logger.Level.Error, $"{JsonKeys.DriveInfoList} not found in response.");
                return;
            }
            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            List<SyncInfo> syncInfos = data.Params[JsonKeys.SyncInfoList].Deserialize<List<SyncInfo>>(options) ?? new List<SyncInfo>();

            // Add/update drives
            foreach (var syncInfo in syncInfos)
            {
                if (syncInfo.DbId is null)
                {
                    Logger.Log(Logger.Level.Error, "syncInfo.DbId is null.");
                    continue;
                }
                await AddOrUpdateSyncInModel(syncInfo);
            }

            // Remove syncs that are no longer present
            var syncDbIds = syncInfos.Where(d => d.DbId != null).Select(d => d.DbId).ToHashSet();

            // Find sync to remove in all users
            var syncsToRemove = new List<Sync>();
            foreach (var drive in _viewModel.AllDrives)
            {
                var driveSyncsToRemove = drive.Syncs.Where(s => !syncDbIds.Contains(s.DbId)).ToList();
                syncsToRemove.AddRange(driveSyncsToRemove);

            }

            await Utility.RunOnUIThread(() =>
            {
                foreach (var sync in syncsToRemove)
                {
                    var parentDrive = sync.Drive;
                    if (parentDrive != null)
                    {
                        parentDrive.Syncs.Remove(sync);
                        Logger.Log(Logger.Level.Info, $"Sync with DbId {sync.DbId} removed from drive DbId {parentDrive.DbId}.");
                    }
                }
            });
        }
        public async Task<bool> AddSync(NewSync newSync, CancellationToken cancellationToken)
        {
            if (newSync.Drive is null)
            {
                Logger.Log(Logger.Level.Error, "NewSync Drive is null.");
                return false;
            }

            JsonObject parms = new()
            {
                [JsonKeys.UserDbId] = newSync.Drive.UserDbId,
                [JsonKeys.AccountId] = newSync.Drive.AccountId,
                [JsonKeys.DriveId] = newSync.Drive.DriveId,
                [JsonKeys.LocalFolderPath] = Utility.ToBase64String(newSync.LocalPath),
                [JsonKeys.ServerFolderPath] = Utility.ToBase64String(newSync.RemotePath),
                [JsonKeys.ServerFolderNodeId] = Utility.ToBase64String(newSync.RemoteNodeId),
                [JsonKeys.LiteSync] = newSync.SyncType == SyncType.Online,
                [JsonKeys.NodeIdList] = JsonSerializer.SerializeToNode(newSync.ExcludedNodeIds, new JsonSerializerOptions { Converters = { new Base64StringJsonConverter() } })
            };
            CommData data = await _commClient.SendRequestAsync(RequestNum.SYNC_ADD, parms, cancellationToken);
            if (data.Params == null || !data.Params.ContainsKey(JsonKeys.SyncInfo))
            {
                Logger.Log(Logger.Level.Error, $"{JsonKeys.SyncInfo} not found in response.");
                return false;
            }

            // Rely on signal to add the sync to the model
            return true;
        }

        public async Task RemoveSync(DbId syncDbId, CancellationToken cancellationToken)
        {
            JsonObject parms = new()
            {
                [JsonKeys.SyncDbId] = syncDbId
            };
            var commData = await _commClient.SendRequestAsync(RequestNum.SYNC_DELETE, parms, cancellationToken);
        }

        public async Task StartSync(DbId syncDbId, CancellationToken cancellationToken)
        {
            Sync? sync = _viewModel.AllSyncs.FirstOrDefault(s => s.DbId == syncDbId);
            if (sync is null)
            {
                Logger.Log(Logger.Level.Error, $"Sync with DbId {syncDbId} not found in model.");
                return;
            }
            sync.SyncStatus = SyncStatus.Starting;

            var parms = new JsonObject
            {
                [JsonKeys.SyncDbId] = syncDbId
            };

            CommData data = await _commClient.SendRequestAsync(RequestNum.SYNC_START, parms, cancellationToken);
            if (data.Params?.ContainsKey(JsonKeys.ExitCode) ?? false && data.Params?[JsonKeys.ExitCode]?.GetValue<int>() != 0)
            {
                Logger.Log(Logger.Level.Error, $"Failed to start sync with DbId {syncDbId}, exit code: {data.Params[JsonKeys.ExitCode]?.GetValue<int>()}");
                return;
            }
        }

        public async Task PauseSync(DbId syncDbId, CancellationToken cancellationToken)
        {
            Sync? sync = _viewModel.AllSyncs.FirstOrDefault(s => s.DbId == syncDbId);
            if (sync is null)
            {
                Logger.Log(Logger.Level.Error, $"Sync with DbId {syncDbId} not found in model.");
                return;
            }
            sync.SyncStatus = SyncStatus.StopAsked;

            var parms = new JsonObject
            {
                [JsonKeys.SyncDbId] = syncDbId
            };
            CommData data = await _commClient.SendRequestAsync(RequestNum.SYNC_STOP, parms, cancellationToken);
            if (data.Params?.ContainsKey(JsonKeys.ExitCode) ?? false && data.Params?[JsonKeys.ExitCode]?.GetValue<int>() != 0)
            {
                Logger.Log(Logger.Level.Error, $"Failed to pause sync with DbId {syncDbId}, exit code: {data.Params[JsonKeys.ExitCode]?.GetValue<int>()}");
                return;
            }
        }

        public async Task RefreshSettings(CancellationToken cancellationToken)
        {
            CommData data = await _commClient.SendRequestAsync(RequestNum.PARAMETERS_INFO, new JsonObject(), cancellationToken);
            if (data.Params == null || !data.Params.ContainsKey(JsonKeys.ParmsInfo))
            {
                Logger.Log(Logger.Level.Error, $"{JsonKeys.ParmsInfo} not found in response.");
                return;
            }
            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            ParmsInfo? parametersInfo = data.Params[JsonKeys.ParmsInfo].Deserialize<ParmsInfo>(options);
            if (parametersInfo == null)
            {
                Logger.Log(Logger.Level.Error, $"Failed to deserialize parmsInfo from ${data.Params["parmsInfo"]}.");
                return;
            }
            CommStruct.ConversionHelper.copyToSettings(parametersInfo, _viewModel.Settings);
        }

        public async Task<List<Node>?> GetSubFolders(DbId userDbId, DriveId driveId, NodeId parentNodeId, CancellationToken cancellationToken)
        {
            var parms = new JsonObject
            {
                [JsonKeys.UserDbId] = userDbId,
                [JsonKeys.DriveId] = driveId,
                [JsonKeys.NodeId] = Utility.ToBase64String(parentNodeId) ?? "",
                [JsonKeys.WithPath] = true
            };
            CommData data = await _commClient.SendRequestAsync(RequestNum.NODE_SUBFOLDERS, parms, cancellationToken);

            if (data.Params == null || !data.Params.ContainsKey(JsonKeys.NodeSubFolderInfoList))
            {
                Logger.Log(Logger.Level.Error, $"{JsonKeys.NodeSubFolderInfoList} not found in response.");
                return new List<Node>();
            }
            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            List<NodeInfo> nodeList = data.Params[JsonKeys.NodeSubFolderInfoList].Deserialize<List<NodeInfo>>(options) ?? new List<NodeInfo>();
            List<Node> nodes = nodeList.Select(n =>
            {
                return new Node(n.NodeId ?? "", n.Name ?? "", n.Size ?? 0, n.ParentNodeId ?? "", n.Path ?? "", userDbId, driveId);
            }).ToList();

            return nodes;
        }
        public async Task<Node?> GetNodeInfo(DbId userDbId, DriveId driveId, NodeId nodeId, CancellationToken cancellationToken)
        {
            var parms = new JsonObject
            {
                [JsonKeys.UserDbId] = userDbId,
                [JsonKeys.DriveId] = driveId,
                [JsonKeys.NodeId] = Utility.ToBase64String(nodeId),
                [JsonKeys.WithPath] = true
            };
            CommData data = await _commClient.SendRequestAsync(RequestNum.NODE_INFO, parms, cancellationToken);

            if (data.Params == null || !data.Params.ContainsKey(JsonKeys.NodeInfo))
            {
                Logger.Log(Logger.Level.Error, $"{JsonKeys.NodeInfo} not found in response.");
                return null;
            }
            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            NodeInfo? nodeInfo = data.Params[JsonKeys.NodeInfo].Deserialize<NodeInfo>(options);
            if (nodeInfo is null)
            {
                Logger.Log(Logger.Level.Error, $"Failed to deserialize nodeInfo from ${data.Params[JsonKeys.NodeInfo]}.");
                return null;
            }
            return new Node(nodeInfo.NodeId ?? "", nodeInfo.Name ?? "", nodeInfo.Size ?? 0, nodeInfo.ParentNodeId ?? "", nodeInfo.Path ?? "", userDbId, driveId);
        }

        public async Task<Int64?> GetFolderSize(long userDbId, long driveId, string nodeId, CancellationToken cancellationToken)
        {
            var parms = new JsonObject
            {
                [JsonKeys.UserDbId] = userDbId,
                [JsonKeys.DriveId] = driveId,
                [JsonKeys.NodeId] = Utility.ToBase64String(nodeId),
            };
            CommData data = await _commClient.SendRequestAsync(RequestNum.NODE_FOLDER_SIZE, parms, cancellationToken);

            if (data.Params == null || !data.Params.ContainsKey(JsonKeys.FolderSize))
            {
                Logger.Log(Logger.Level.Error, $"{JsonKeys.FolderSize} not found in response.");
                return -1;
            }

            Int64? folderSize = data.Params[JsonKeys.FolderSize]?.GetValue<Int64>();
            return folderSize;
        }

        public async Task<List<NodeId>?> GetBlacklistedNodeIdList(DbId syncDbId, CancellationToken cancellationToken)
        {
            var parms = new JsonObject
            {
                [JsonKeys.SyncDbId] = syncDbId,
            };
            CommData data = await _commClient.SendRequestAsync(RequestNum.BLACKLISTED_NODE_LIST, parms, cancellationToken);

            if (data.Params == null || !data.Params.ContainsKey(JsonKeys.NodeIdList))
            {
                Logger.Log(Logger.Level.Error, $"{JsonKeys.NodeIdList} not found in response.");
                return null;
            }
            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            return data.Params[JsonKeys.NodeIdList].Deserialize<List<NodeId>>(options) ?? new List<NodeId>();
        }

        public async Task SetBlacklistedNodeIdList(DbId syncDbId, List<NodeId> idList, CancellationToken cancellationToken)
        {
            var parms = new JsonObject
            {
                [JsonKeys.SyncDbId] = syncDbId,
                [JsonKeys.NodeIdList] = JsonSerializer.SerializeToNode(idList, new JsonSerializerOptions { Converters = { new Base64StringJsonConverter() } })
            };
            CommData data = await _commClient.SendRequestAsync(RequestNum.BLACKLISTED_NODE_SETLIST, parms, cancellationToken);

            if (data.Params is not null && data.Params.ContainsKey(JsonKeys.ExitCode) && data.Params[JsonKeys.ExitCode]?.GetValue<int>() != 0)
            {
                Logger.Log(Logger.Level.Error, $"Failed to set sync node id list for syncDbId {syncDbId}, exit code: {data.Params[JsonKeys.ExitCode]?.GetValue<int>()}");
                return;
            }
        }
        public async Task StartUpdate(CancellationToken cancellationToken)
        {
            await _commClient.SendRequestAsync(RequestNum.UPDATER_START_INSTALLER, new JsonObject(), cancellationToken);
        }
        public async Task RefreshUpdaterVersionInfo(CancellationToken cancellationToken)
        {
            var channel = _viewModel.Settings.UpdateManager.CurrentChannel;
            var parms = new JsonObject
            {
                [JsonKeys.UpdateChannel] = (int)channel
            };
            CommData data = await _commClient.SendRequestAsync(RequestNum.UPDATER_VERSION_INFO, parms, cancellationToken);
            if (data.Params == null || !data.Params.ContainsKey(JsonKeys.VersionInfo))
            {
                Logger.Log(Logger.Level.Error, $"{JsonKeys.VersionInfo} not found in response.");
                return;
            }

            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            AppVersion? versionInfo = data.Params[JsonKeys.VersionInfo].Deserialize<AppVersion>(options);
            if (versionInfo?.Tag == _viewModel.Settings.AppVersion?.Tag && versionInfo?.BuildVersion == _viewModel.Settings.AppVersion?.BuildVersion)
            {
                _viewModel.Settings.UpdateManager.AvailableUpdate = null;
                return;
            }
            _viewModel.Settings.UpdateManager.AvailableUpdate = versionInfo;
        }
        public async Task ChangeUpdaterChannel(VersionChannel newChannel, CancellationToken cancellationToken)
        {
            _viewModel.Settings.UpdateManager.AvailableUpdate = null;
            await _commClient.SendRequestAsync(RequestNum.UPDATER_CHANGE_CHANNEL, new JsonObject { [JsonKeys.UpdateChannel] = (int)newChannel }, cancellationToken);
            _viewModel.Settings.UpdateManager.CurrentChannel = newChannel;
        }

        public async Task SaveSettings(CancellationToken cancellationToken)
        {
            ParmsInfo ParmsInfo = new();
            CommStruct.ConversionHelper.copyToParmsInfo(_viewModel.Settings, ParmsInfo);
            JsonObject parms = new()
            {
                [JsonKeys.ParmsInfo] = new JsonObject()
            };

            string ParmsInfoJson = JsonSerializer.Serialize(ParmsInfo);
            parms[JsonKeys.ParmsInfo] = JsonNode.Parse(ParmsInfoJson) ?? new JsonObject();
            await _commClient.SendRequestAsync(RequestNum.PARAMETERS_UPDATE, parms, cancellationToken);
        }

        // Signals
        public async void OnSignalReceived(object? sender, SignalEventArgs args)
        {
            switch (args.SignalNum)
            {
                case SignalNum.USER_UPDATED:
                case SignalNum.USER_ADDED:
                    await HandleUserUpdatedOrAddedAsync(sender, args);
                    break;
                case SignalNum.USER_REMOVED:
                    await HandleUserRemovedAsync(sender, args);
                    break;
                case SignalNum.ACCOUNT_ADDED:
                case SignalNum.ACCOUNT_UPDATED:
                    await HandleAccountUpdatedOrAddedAsync(sender, args);
                    break;
                case SignalNum.ACCOUNT_REMOVED:
                    await HandleAccountRemovedAsync(sender, args);
                    break;
                case SignalNum.DRIVE_ADDED:
                case SignalNum.DRIVE_UPDATED:
                    await HandleDriveUpdatedOrAddedAsync(sender, args);
                    break;
                case SignalNum.DRIVE_REMOVED:
                    await HandleDriveRemovedAsync(sender, args);
                    break;
                case SignalNum.SYNC_ADDED:
                case SignalNum.SYNC_UPDATED:
                    await HandleSyncUpdatedOrAddedAsync(sender, args);
                    break;
                case SignalNum.SYNC_REMOVED:
                    await HandleSyncRemovedAsync(sender, args);
                    break;
                case SignalNum.UPDATER_STATE_CHANGED:
                    await HandleUpdaterStateChangedAsync(sender, args);
                    break;
                case SignalNum.SYNC_PROGRESSINFO:
                    await HandleSyncProgressInfo(sender, args);
                    break;
                case SignalNum.SYNC_COMPLETEDITEM:
                    await HandleSyncCompletedItem(sender, args);
                    break;
                case SignalNum.UTILITY_ERROR_ADDED:
                    await HandleErrorAddedAsync(sender, args);
                    break;
                case SignalNum.UTILITY_ERRORS_REMOVED:
                    await HandleErrorRemovedAsync(sender, args);
                    break;
                default:
                    Logger.Log(Logger.Level.Warning, $"Unhandled signal received: {args.SignalNum}");
                    break;
            }
        }

        public async Task HandleUserUpdatedOrAddedAsync(object? sender, SignalEventArgs args)
        {
            var signalData = args.SignalData;

            if (signalData == null || !signalData.ContainsKey(JsonKeys.UserInfo))
            {
                Logger.Log(Logger.Level.Error, $"{JsonKeys.UserInfo} not found in parameters.");
                return;
            }

            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            UserInfo? newUserInfo = signalData[JsonKeys.UserInfo].Deserialize<UserInfo>(options);
            if (newUserInfo == null)
            {
                Logger.Log(Logger.Level.Error, $"Failed to deserialize userInfo from ${signalData["userInfo"]}.");
                return;
            }
            if (newUserInfo?.DbId is null)
            {
                Logger.Log(Logger.Level.Error, "userInfo.DbId is null.");
                return;
            }
            await AddOrUpdateUserInModel(newUserInfo);
        }

        public async Task HandleAccountUpdatedOrAddedAsync(object? sender, SignalEventArgs args)
        {
            var signalData = args.SignalData;

            if (signalData == null || !signalData.ContainsKey(JsonKeys.AccountInfo))
            {
                Logger.Log(Logger.Level.Error, $"{JsonKeys.AccountInfo} not found in parameters.");
                return;
            }

            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            AccountInfo accountInfo = signalData[JsonKeys.AccountInfo].Deserialize<AccountInfo>(options) ?? new AccountInfo();

            if (accountInfo.DbId is null)
            {
                Logger.Log(Logger.Level.Error, "accountInfo.DbId is null.");
                return;
            }
            await AddOrUpdateAccountInModel(accountInfo);
        }
        public async Task HandleAccountRemovedAsync(object? sender, SignalEventArgs args)
        {
            var signalData = args.SignalData;

            if (signalData == null || !signalData.ContainsKey(JsonKeys.AccountDbId))
            {
                Logger.Log(Logger.Level.Error, $"{JsonKeys.AccountDbId} not found in parameters.");
                return;
            }

            DbId? accountDbId = signalData[JsonKeys.AccountDbId]?.AsValue().GetValue<DbId>();

            if (accountDbId is null)
            {
                Logger.Log(Logger.Level.Error, "accountDbId is null.");
                return;
            }

            Account? deletedAccount = _viewModel.Users.SelectMany(u => u.Accounts).FirstOrDefault(a => a.DbId == accountDbId);
            if (deletedAccount == null)
            {
                Logger.Log(Logger.Level.Error, $"Account with dbID {accountDbId} not found in the model.");
                return;
            }
            await Utility.RunOnUIThread(() => { deletedAccount.User.Accounts.Remove(deletedAccount); });
        }
        public async Task HandleDriveUpdatedOrAddedAsync(object? sender, SignalEventArgs args)
        {
            var signalData = args.SignalData;

            if (signalData == null || !signalData.ContainsKey(JsonKeys.DriveInfo))
            {
                Logger.Log(Logger.Level.Error, $"{JsonKeys.DriveInfo} not found in parameters.");
                return;
            }

            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            options.Converters.Add(new ColorJsonConverter());
            DriveInfo driveInfo = signalData[JsonKeys.DriveInfo].Deserialize<DriveInfo>(options) ?? new DriveInfo();

            if (driveInfo.DbId is null)
            {
                Logger.Log(Logger.Level.Error, "driveInfo.DbId is null.");
                return;
            }
            await AddOrUpdateDriveInModel(driveInfo);
        }
        public async Task HandleDriveRemovedAsync(object? sender, SignalEventArgs args)
        {
            var signalData = args.SignalData;

            if (signalData == null || !signalData.ContainsKey(JsonKeys.DriveDbId))
            {
                Logger.Log(Logger.Level.Error, $"{JsonKeys.DriveDbId} not found in parameters.");
                return;
            }

            DbId? driveDbId = signalData[JsonKeys.DriveDbId]?.AsValue().GetValue<DbId>();

            if (driveDbId is null)
            {
                Logger.Log(Logger.Level.Error, "driveDbId is null.");
                return;
            }

            Drive? deletedDrive = _viewModel.AllDrives.FirstOrDefault(d => d.DbId == driveDbId);
            if (deletedDrive == null)
            {
                Logger.Log(Logger.Level.Error, $"Drive with dbID {driveDbId} not found in the model.");
                return;
            }
            await Utility.RunOnUIThread(() => { deletedDrive.Account.Drives.Remove(deletedDrive); });
        }

        public async Task HandleSyncUpdatedOrAddedAsync(object? sender, SignalEventArgs args)
        {
            var signalData = args.SignalData;

            if (signalData == null || !signalData.ContainsKey(JsonKeys.SyncInfo))
            {
                Logger.Log(Logger.Level.Error, $"{JsonKeys.SyncInfo} not found in parameters.");
                return;
            }

            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            SyncInfo syncInfo = signalData[JsonKeys.SyncInfo].Deserialize<SyncInfo>(options) ?? new SyncInfo();

            if (syncInfo.DbId is null)
            {
                Logger.Log(Logger.Level.Error, "syncInfo.DbId is null.");
                return;
            }
            await AddOrUpdateSyncInModel(syncInfo);
        }

        public async Task HandleSyncRemovedAsync(object? sender, SignalEventArgs args)
        {
            var signalData = args.SignalData;

            if (signalData == null || !signalData.ContainsKey(JsonKeys.SyncDbId))
            {
                Logger.Log(Logger.Level.Error, $"{JsonKeys.SyncDbId} not found in parameters.");
                return;
            }

            DbId? syncDbID = signalData[JsonKeys.SyncDbId]?.AsValue().GetValue<DbId>();

            if (syncDbID is null)
            {
                Logger.Log(Logger.Level.Error, "syncDbID is null.");
                return;
            }

            Sync? deletedSync = _viewModel.AllSyncs.FirstOrDefault(s => s.DbId == syncDbID);
            if (deletedSync == null)
            {
                Logger.Log(Logger.Level.Error, $"Sync with dbID {syncDbID} not found in the model.");
                return;
            }
            await Utility.RunOnUIThread(() => { deletedSync.Drive.Syncs.Remove(deletedSync); });
        }
        public async Task HandleSyncProgressInfo(object? sender, SignalEventArgs args)
        {
            var signalData = args.SignalData;

            if (signalData == null || !signalData.ContainsKey(JsonKeys.SyncDbId))
            {
                Logger.Log(Logger.Level.Error, $"{JsonKeys.SyncDbId} not found in parameters.");
                return;
            }
            if (signalData == null || !signalData.ContainsKey(JsonKeys.SyncStatus))
            {
                Logger.Log(Logger.Level.Error, $"{JsonKeys.SyncStatus} not found in parameters.");
                return;
            }

            DbId? syncDbID = signalData[JsonKeys.SyncDbId]?.AsValue().GetValue<DbId>();
            SyncStatus? syncStatus = signalData[JsonKeys.SyncStatus]?.Deserialize<SyncStatus>();

            if (syncDbID is null)
            {
                Logger.Log(Logger.Level.Error, "syncDbID is null.");
                return;
            }
            if (syncStatus is null)
            {
                Logger.Log(Logger.Level.Error, "syncStatus is null.");
                return;
            }

            Sync? updatedSync = _viewModel.AllSyncs.FirstOrDefault(s => s.DbId == syncDbID);
            if (updatedSync == null)
            {
                Logger.Log(Logger.Level.Error, $"Sync with dbID {syncDbID} not found in the model.");
                return;
            }
            updatedSync.SyncStatus = syncStatus ?? SyncStatus.Undefined;
        }

        public async Task HandleSyncCompletedItem(object? sender, SignalEventArgs args)
        {
            var signalData = args.SignalData;

            if (signalData == null || !signalData.ContainsKey(JsonKeys.SyncDbId))
            {
                Logger.Log(Logger.Level.Error, $"{JsonKeys.SyncDbId} not found in parameters.");
                return;
            }
            if (signalData == null || !signalData.ContainsKey(JsonKeys.SyncFileItemInfo))
            {
                Logger.Log(Logger.Level.Error, $"{JsonKeys.SyncFileItemInfo} not found in parameters.");
                return;
            }

            DbId? syncDbID = signalData[JsonKeys.SyncDbId]?.AsValue().GetValue<DbId>();
            if (syncDbID is null)
            {
                Logger.Log(Logger.Level.Error, "syncDbID is null.");
                return;
            }
            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            SyncFileItemInfo? fileItemInfo = signalData[JsonKeys.SyncFileItemInfo]?.Deserialize<SyncFileItemInfo>(options);
            if (fileItemInfo is null)
            {
                Logger.Log(Logger.Level.Error, "fileItemInfo is null.");
                return;
            }

            Sync? sync = _viewModel.AllSyncs.FirstOrDefault(s => s.DbId == syncDbID);
            if (sync == null)
            {
                Logger.Log(Logger.Level.Error, $"Sync with dbID {syncDbID} not found in the model.");
                return;
            }

            SyncFileItem syncFileItem = new SyncFileItem(sync);
            CommStruct.ConversionHelper.copyToSyncFileItem(fileItemInfo, syncFileItem);
            await Utility.RunOnUIThread(() => { sync.SyncActivities.Insert(0, syncFileItem); });
        }

        public async Task HandleUpdaterStateChangedAsync(object? sender, SignalEventArgs args)
        {
            await RefreshUpdaterVersionInfo(new CancellationToken());
        }

        public async Task HandleUserRemovedAsync(object? sender, SignalEventArgs args)
        {
            var signalData = args.SignalData;

            if (signalData == null || !signalData.ContainsKey(JsonKeys.UserDbId))
            {
                Logger.Log(Logger.Level.Error, $"{JsonKeys.UserDbId} not found in parameters.");
                return;
            }
            DbId dbId = signalData[JsonKeys.UserDbId]?.GetValue<DbId>() ?? -1;

            User? user = _viewModel.Users.FirstOrDefault(u => u.DbId == dbId);
            if (user == null)
            {
                Logger.Log(Logger.Level.Error, $"User with dbId {dbId} not found");
                return;
            }
            await Utility.RunOnUIThread(() => { _viewModel.Users.Remove(user); });
            return;
        }

        public async Task HandleErrorAddedAsync(object? sender, SignalEventArgs args)
        {
            var signalData = args.SignalData;

            if (signalData == null || !signalData.ContainsKey(JsonKeys.ErrorInfo))
            {
                Logger.Log(Logger.Level.Error, $"{JsonKeys.ErrorInfo} not found in parameters.");
                return;
            }

            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            ErrorInfo? errorInfo = signalData[JsonKeys.ErrorInfo]?.Deserialize<ErrorInfo>(options);
            Error error = new Error();
            if (errorInfo == null)
            {
                Logger.Log(Logger.Level.Error, $"Failed to deserialize errorInfo from ${signalData[JsonKeys.ErrorInfo]}.");
                return;
            }

            ConversionHelper.copyToError(errorInfo, error);
            await _viewModel.AddError(error);
        }
        public async Task HandleErrorRemovedAsync(object? sender, SignalEventArgs args)
        {
            var signalData = args.SignalData;
            if (signalData == null || !signalData.ContainsKey(JsonKeys.ErrorDbId))
            {
                Logger.Log(Logger.Level.Error, $"{JsonKeys.ErrorDbId} not found in parameters.");
                return;
            }

            DbId? errorDbId = signalData[JsonKeys.ErrorDbId]?.AsValue().GetValue<DbId>();
            if (errorDbId is null)
            {
                Logger.Log(Logger.Level.Error, "errorDbId is null.");
                return;
            }
            await _viewModel.RemoveErrorByDbId(errorDbId.Value);
        }

        // Helpers
        private async Task AddOrUpdateUserInModel(UserInfo userInfo)
        {
            if (_viewModel.Users.Any(u => u.DbId == userInfo.DbId))
            {
                Logger.Log(Logger.Level.Info, $"User with DbId {userInfo.DbId} already exists in the application.");
                var user = _viewModel.Users.FirstOrDefault(u => u.DbId == userInfo.DbId);
                if (user == null)
                {
                    Logger.Log(Logger.Level.Error, $"Unexpected error, user with DbId {userInfo.DbId} not found after existence check.");
                    return;
                }
                ConversionHelper.copyToUser(userInfo, user);
                Logger.Log(Logger.Level.Info, $"User with DbId {userInfo.DbId} updated.");
            }
            else
            {
                User newUser = new User(userInfo.DbId ?? throw new InvalidOperationException("DbId should not be null here."));
                ConversionHelper.copyToUser(userInfo, newUser);
                await Utility.RunOnUIThread(() =>
                {
                    _viewModel.Users.Add(newUser);
                });
                Logger.Log(Logger.Level.Info, $"New user added with DbId {newUser.DbId}.");
            }
        }

        private async Task AddOrUpdateAccountInModel(AccountInfo accountInfo)
        {
            await AutoRetry(async Task<bool> () =>
            {
                foreach (var user in _viewModel.Users)
                {
                    var account = user.Accounts.FirstOrDefault(a => a.DbId == accountInfo.DbId);
                    if (account == null)
                    {
                        continue;
                    }
                    ConversionHelper.copyToAccount(accountInfo, account);
                    Logger.Log(Logger.Level.Info, $"Account with DbId {accountInfo.DbId} updated.");
                    return true;
                }

                // Account not found, add it to the correct user
                DbId? userDbId = accountInfo.UserDbId;
                var parentUser = _viewModel.Users.FirstOrDefault(u => u.DbId == userDbId);
                if (parentUser == null)
                {
                    // This might happen due to asynchronous signal processing
                    Logger.Log(Logger.Level.Error, $"Parent user with DbId {userDbId} not found for account DbId {accountInfo.DbId}.");
                    return false;
                }

                var newAccount = new Account(accountInfo.DbId ?? throw new InvalidOperationException("DbId should not be null here."), parentUser);
                ConversionHelper.copyToAccount(accountInfo, newAccount);
                await Utility.RunOnUIThread(() => { parentUser.Accounts.Add(newAccount); });
                Logger.Log(Logger.Level.Info, $"New account added to user with DbId {userDbId}.");
                return true;
            });

        }

        private async Task AddOrUpdateDriveInModel(DriveInfo driveInfo)
        {
            await AutoRetry(async Task<bool> () =>
            {
                foreach (var user in _viewModel.Users)
                {

                    var drive = user.Drives.FirstOrDefault(d => d?.DbId == driveInfo.DbId, null);
                    if (drive == null)
                    {
                        continue;
                    }
                    ConversionHelper.copyToDrive(driveInfo, drive);
                    Logger.Log(Logger.Level.Info, $"Drive with DbId {driveInfo.DbId} updated.");
                    return true;
                }
                // Drive not found, add it to the correct account
                DbId? accountDbId = driveInfo.AccountDbId; // Assuming DriveInfo has an AccountDbId property
                Account? parentAccount = null;
                foreach (var user in _viewModel.Users)
                {
                    parentAccount = user.Accounts.FirstOrDefault(a => a.DbId == accountDbId);
                    if (parentAccount != null)
                        break;
                }
                if (parentAccount == null)
                {
                    // This might happen due to asynchronous signal processing
                    Logger.Log(Logger.Level.Error, $"Parent account with DbId {accountDbId} not found for drive DbId {driveInfo.DbId}.");
                    return false;
                }
                await Utility.RunOnUIThread(() =>
                {
                    Drive newDrive = new Drive(driveInfo.DbId ?? throw new InvalidOperationException("DbId should not be null here."), parentAccount);
                    ConversionHelper.copyToDrive(driveInfo, newDrive);
                    parentAccount.Drives.Add(newDrive);

                });
                Logger.Log(Logger.Level.Info, $"New drive added to account with DbId {accountDbId}.");
                return true;
            });
        }

        private async Task AddOrUpdateSyncInModel(SyncInfo syncInfo)
        {
            await AutoRetry(async Task<bool> () =>
            {
                List<Drive> allDrives = new();
                foreach (var user in _viewModel.Users)
                {
                    allDrives.AddRange(user.Drives);
                }

                foreach (var drive in allDrives)
                {
                    var sync = drive.Syncs.FirstOrDefault(s => s?.DbId == syncInfo.DbId, null);
                    if (sync == null)
                    {
                        continue;
                    }
                    ConversionHelper.copyToSync(syncInfo, sync);
                    Logger.Log(Logger.Level.Info, $"Sync with DbId {syncInfo.DbId} updated.");
                    return true;
                }

                // Sync not found, add it to the correct drive
                DbId? driveDbId = syncInfo.DriveDbId; // Assuming SyncInfo has a DriveDbId property
                var parentDrive = allDrives.FirstOrDefault(d => d.DbId == driveDbId);
                if (parentDrive == null)
                {
                    // This might happen due to asynchronous signal processing
                    Logger.Log(Logger.Level.Error, $"Parent drive with DbId {driveDbId} not found for sync DbId {syncInfo.DbId}.");
                    return false;
                }
                await Utility.RunOnUIThread(() =>
                {
                    Sync newSync = new Sync(syncInfo.DbId ?? throw new InvalidOperationException("DbId should not be null here."), parentDrive);
                    ConversionHelper.copyToSync(syncInfo, newSync);
                    parentDrive.Syncs.Add(newSync);
                });
                Logger.Log(Logger.Level.Info, $"New sync added to drive with DbId {driveDbId}.");
                return true;
            });
        }

        private async Task AutoRetry(Func<Task<bool>> action, int maxRetries = 3, int delayMilliseconds = 1000, [CallerMemberName] string callerName = "?")
        {
            int attempt = 0;
            while (attempt < maxRetries)
            {
                if (await action())
                {
                    return;
                }

                attempt++;
                if (attempt < maxRetries)
                {
                    await Task.Delay(delayMilliseconds);
                }
            }
            Logger.Log(Logger.Level.Error, $"AutoRetry: Failed to complete action in {callerName} after {maxRetries} attempts separated by {delayMilliseconds}ms.");
        }
    }
}


