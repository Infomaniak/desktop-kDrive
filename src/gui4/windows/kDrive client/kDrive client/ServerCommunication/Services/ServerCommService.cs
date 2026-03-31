using DynamicData;
using Infomaniak.kDrive.ServerCommunication.CommStruct;
using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.ServerCommunication.JsonConverters;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
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
using static Infomaniak.kDrive.ServerCommunication.Interfaces.IServerCommService;

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

        // Utility methods
        private static bool HasRequiredParam(CommData data, string key, [CallerMemberName] string callerName = "")
        {
            if (data.Params is null || !data.Params.ContainsKey(key))
            {
                Logger.Log(Logger.Level.Error, $"{callerName}: {key} not found in response.");
                return false;
            }
            return true;
        }

        private static bool CheckJobResultAndLogIfError(CommData? data, JsonObject? jobInput = null, [CallerMemberName] string callerName = "")
        {
            if (data is null)
            {
                Logger.Log(Logger.Level.Error, $"Job result check failed at {callerName} with input {jobInput}, CommData is null.");
                return false;
            }

            if (data.Params is null)
            {
                Logger.Log(Logger.Level.Error, $"Job result check failed at {callerName} with input {jobInput}, Params is null.");
                return false;
            }

            if (data.Code != ExitCode.Ok)
            {
                Logger.Log(Logger.Level.Error, $"Job result check failed at {callerName} with input {jobInput}, exit code: {data.Code}, exit cause: {data.Cause}.");
                return false;
            }
            return true;
        }

        // Requests
        public async Task<List<DbId>?> GetUserDbIds(CancellationToken cancellationToken)
        {
            CommData data = await _commClient.SendRequestAsync(RequestNum.USER_DBIDLIST, [], cancellationToken);
            if (data.Params is null || !data.Params.ContainsKey(JsonKeys.UserDbIds))
            {
                Logger.Log(Logger.Level.Error, $"{JsonKeys.UserDbIds} not found in response.");
                return null;
            }
            return data.Params[JsonKeys.UserDbIds].Deserialize<List<DbId>>();
        }

        public async Task<User?> AddOrRelogUser(string oAuthCode, string oAuthCodeVerifier, CancellationToken cancellationToken)
        {
            JsonObject parms = new()
            {
                [JsonKeys.OAuthCode] = Utility.ToBase64String(oAuthCode),
                [JsonKeys.OAuthCodeVerifier] = Utility.ToBase64String(oAuthCodeVerifier)
            };

            CommData data = await _commClient.SendRequestAsync(RequestNum.LOGIN_REQUESTTOKEN, parms, cancellationToken).ConfigureAwait(false);

            if (!CheckJobResultAndLogIfError(data, parms))
                return null;

            if (!HasRequiredParam(data, JsonKeys.UserDbId))
                return null;

            DbId userDbId = -1;
            try
            {
                userDbId = data.Params[JsonKeys.UserDbId]!.GetValue<DbId>();
            }
            catch (Exception ex)
            {
                Logger.Log(Logger.Level.Error, $"Failed to parse UserDbId from response: {ex.Message}");
                return null;
            }

            await Utility.RunOnUIThread(async () =>
            {
                // The server should send a signal user_added that will add the user to the model, we wait here for max 10s for that to happen
                int maxRetries = 100;
                do
                {
                    if (_viewModel.Users.Any(u => u.DbId == userDbId))
                    {
                        Logger.Log(Logger.Level.Info, $"AddOrRelogUser: User with DbId {userDbId} already exists in the application.");
                        return;
                    }
                    await Task.Delay(100, cancellationToken).ConfigureAwait(false);
                    --maxRetries;
                } while (!cancellationToken.IsCancellationRequested && maxRetries > 0);
                Logger.Log(Logger.Level.Error, $"AddOrRelogUser: Timeout waiting for user with DbId {userDbId} to be added to the application.");
            }).ConfigureAwait(false);
            return _viewModel.Users.FirstOrDefault(u => u?.DbId == userDbId, null);
        }

        public async Task<bool> RefreshUsers(CancellationToken cancellationToken)
        {
            CommData data = await _commClient.SendRequestAsync(RequestNum.USER_INFOLIST, [], cancellationToken).ConfigureAwait(false);

            if (!CheckJobResultAndLogIfError(data, []))
                return false;

            if (!HasRequiredParam(data, JsonKeys.UserInfoList))
                return false;

            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            List<UserInfo>? userInfos = data.Params[JsonKeys.UserInfoList].Deserialize<List<UserInfo>>(options);
            if (userInfos is null)
            {
                Logger.Log(Logger.Level.Error, "Failed to deserialize UserInfoList.");
                return false;
            }

            // Add/update users
            foreach (var userInfo in userInfos)
            {
                if (userInfo.DbId is null)
                {
                    Logger.Log(Logger.Level.Error, "userInfo.DbId is null.");
                    continue;
                }
                await AddOrUpdateUserInModel(userInfo).ConfigureAwait(false);
            }

            // Remove users that are no longer present
            var userDbIds = userInfos.Where(u => u.DbId != null).Select(u => u.DbId).ToHashSet();
            await Utility.RunOnUIThread(() =>
            {
                var usersToRemove = _viewModel.Users.Where(u => !userDbIds.Contains(u.DbId)).ToList();
                _viewModel.Users.RemoveMany(usersToRemove);
                Logger.Log(Logger.Level.Info, $"{usersToRemove.Count} users removed from the application.");
            }).ConfigureAwait(false);
            return true;
        }

        public async Task<bool> RemoveUser(DbId userDbId, CancellationToken cancellationToken)
        {
            JsonObject parms = new()
            {
                [JsonKeys.UserDbId] = userDbId
            };
            var commData = await _commClient.SendRequestAsync(RequestNum.USER_DELETE, parms, cancellationToken).ConfigureAwait(false);

            // If the user is not found on the server, we assume it is already deleted and remove it from the model
            if (commData?.Code == ExitCode.DataError && commData?.Cause == ExitCause.DbEntryNotFound)
            {
                Logger.Log(Logger.Level.Warning, $"User with DbId {userDbId} not found on server, assuming already deleted.");
                await Utility.RunOnUIThread(() =>
                {
                    var userToRemove = _viewModel.Users.FirstOrDefault(u => u.DbId == userDbId);
                    if (userToRemove is not null)
                        _viewModel.Users.Remove(userToRemove);
                    Logger.Log(Logger.Level.Info, $"User with DbId {userDbId} removed.");
                }).ConfigureAwait(false);
                return true;
            }

            return CheckJobResultAndLogIfError(commData, parms);
        }

        public async Task<bool> RefreshAccounts(CancellationToken cancellationToken)
        {
            CommData data = await _commClient.SendRequestAsync(RequestNum.ACCOUNT_INFOLIST, [], cancellationToken).ConfigureAwait(false);
            if (!CheckJobResultAndLogIfError(data))
                return false;

            if (!HasRequiredParam(data, JsonKeys.AccountInfoList))
                return false;

            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            List<AccountInfo>? accountInfos = data.Params[JsonKeys.AccountInfoList].Deserialize<List<AccountInfo>>(options);
            if (accountInfos is null)
            {
                Logger.Log(Logger.Level.Error, "Failed to deserialize AccountInfoList.");
                return false;
            }

            // Add/update accounts
            foreach (var accountInfo in accountInfos)
            {
                if (accountInfo.DbId is null)
                {
                    Logger.Log(Logger.Level.Error, "accountInfo.DbId is null.");
                    continue;
                }
                await AddOrUpdateAccountInModel(accountInfo).ConfigureAwait(false);
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
            }).ConfigureAwait(false);
            return true;
        }

        public async Task<bool> RefreshDrives(CancellationToken cancellationToken)
        {
            CommData data = await _commClient.SendRequestAsync(RequestNum.DRIVE_INFOLIST, [], cancellationToken).ConfigureAwait(false);
            if (!CheckJobResultAndLogIfError(data))
                return false;

            if (!HasRequiredParam(data, JsonKeys.DriveInfoList))
                return false;

            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            options.Converters.Add(new ColorJsonConverter());
            List<DriveInfo>? driveInfos = data.Params[JsonKeys.DriveInfoList].Deserialize<List<DriveInfo>>(options);
            if (driveInfos is null)
            {
                Logger.Log(Logger.Level.Error, "Failed to deserialize DriveInfoList.");
                return false;
            }

            // Add/update drives
            foreach (var driveInfo in driveInfos)
            {
                if (driveInfo.DbId is null)
                {
                    Logger.Log(Logger.Level.Error, "driveInfo.DbId is null.");
                    continue;
                }
                await AddOrUpdateDriveInModel(driveInfo).ConfigureAwait(false);
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
            }).ConfigureAwait(false);
            return true;
        }

        public async Task<bool> RefreshUserDrivesAvailable(DbId userDbId, CancellationToken cancellationToken)
        {
            JsonObject parms = new()
            {
                [JsonKeys.UserDbId] = userDbId
            };
            CommData data = await _commClient.SendRequestAsync(RequestNum.USER_AVAILABLEDRIVES, parms, cancellationToken).ConfigureAwait(false);
            if (!CheckJobResultAndLogIfError(data, parms))
                return false;

            if (!HasRequiredParam(data, JsonKeys.DriveAvailableInfoList))
                return false;

            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            options.Converters.Add(new ColorJsonConverter());
            List<DriveAvailableInfo>? driveAvailableInfoList = data.Params[JsonKeys.DriveAvailableInfoList].Deserialize<List<DriveAvailableInfo>>(options);
            if (driveAvailableInfoList is null)
            {
                Logger.Log(Logger.Level.Error, "Failed to deserialize DriveAvailableInfoList.");
                return false;
            }


            User? user = _viewModel.Users.FirstOrDefault<User>(u => u.DbId == userDbId);
            if (user is null)
            {
                Logger.Log(Logger.Level.Error, $"User not found with dbID {userDbId}.");
                return false;
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
                    CommStruct.ConversionHelper.CopyToDriveAvailable(info, drive);
                    await Utility.RunOnUIThread(() => { user.DrivesAvailable.Add(drive); }).ConfigureAwait(false);
                    continue;
                }

                // Check if any of the properties have changed, if yes remove and re-add to trigger UI update
                var existingDrive = user.DrivesAvailable.FirstOrDefault(d => d.DriveId == info.DriveId);
                if (existingDrive is null)
                    continue;

                var tempDrive = new DriveAvailable();
                CommStruct.ConversionHelper.CopyToDriveAvailable(info, tempDrive);
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
                        }).ConfigureAwait(false);
                        break;
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
                        await Utility.RunOnUIThread(() => { user.DrivesAvailable.Remove(driveToRemove); }).ConfigureAwait(false);
                    }
                }
            }
            return true;
        }

        public async Task<bool> RefreshSyncs(CancellationToken cancellationToken)
        {
            CommData data = await _commClient.SendRequestAsync(RequestNum.SYNC_INFOLIST, [], cancellationToken).ConfigureAwait(false);
            if (!CheckJobResultAndLogIfError(data))
                return false;

            if (!HasRequiredParam(data, JsonKeys.SyncInfoList))
                return false;

            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            List<SyncInfo>? syncInfos = data.Params[JsonKeys.SyncInfoList].Deserialize<List<SyncInfo>>(options);
            if (syncInfos is null)
            {
                Logger.Log(Logger.Level.Error, "Failed to deserialize SyncInfoList.");
                return false;
            }

            // Add/update drives
            foreach (var syncInfo in syncInfos)
            {
                if (syncInfo.DbId is null)
                {
                    Logger.Log(Logger.Level.Error, "syncInfo.DbId is null.");
                    continue;
                }
                await AddOrUpdateSyncInModel(syncInfo).ConfigureAwait(false);
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
            }).ConfigureAwait(false);
            return true;
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
                [JsonKeys.BlackList] = JsonSerializer.SerializeToNode(newSync.ExcludedNodeIds, new JsonSerializerOptions { Converters = { new Base64StringJsonConverter() } })
            };
            CommData data = await _commClient.SendRequestAsync(RequestNum.SYNC_ADD, parms, cancellationToken).ConfigureAwait(false);
            if (!CheckJobResultAndLogIfError(data, parms))
                return false;

            if (!HasRequiredParam(data, JsonKeys.SyncInfo))
                return false;

            // Rely on signal to add the sync to the model
            return true;
        }

        public async Task<bool> SetSyncType(DbId syncDbId, SyncType type, CancellationToken cancellationToken)
        {
            Sync? sync = _viewModel.AllSyncs.FirstOrDefault(s => s.DbId == syncDbId);
            if (sync is null)
            {
                Logger.Log(Logger.Level.Error, $"Sync with DbId {syncDbId} not found in model.");
                return false;
            }

            if (type == SyncType.Online)
            {
                // Ensure the path supports online mode
                bool? supportsLiteSync = await CanPathSupportLiteSync(sync.LocalPath, CancellationToken.None);
                if (!supportsLiteSync.HasValue || !supportsLiteSync.Value)
                {
                    Logger.Log(Logger.Level.Warning, $"Cannot set sync DbId {syncDbId} to online mode, local path does not support it.");
                    return false;
                }
            }

            var parms = new JsonObject
            {
                [JsonKeys.SyncDbId] = syncDbId,
                [JsonKeys.Value] = type == SyncType.Online
            };

            CommData data = await _commClient.SendRequestAsync(RequestNum.SYNC_SETSUPPORTSVIRTUALFILES, parms, cancellationToken).ConfigureAwait(false);
            if (!CheckJobResultAndLogIfError(data, parms))
                return false;

            return true;
        }

        public async Task<bool> RemoveSync(DbId syncDbId, CancellationToken cancellationToken)
        {
            JsonObject parms = new()
            {
                [JsonKeys.SyncDbId] = syncDbId
            };
            var commData = await _commClient.SendRequestAsync(RequestNum.SYNC_DELETE, parms, cancellationToken).ConfigureAwait(false);

            // If the sync is not found on the server, we assume it is already deleted and remove it from the model
            if (commData?.Code == ExitCode.DataError && commData?.Cause == ExitCause.DbEntryNotFound)
            {
                Logger.Log(Logger.Level.Info, $"Sync with DbId {syncDbId} not found on server, assuming already deleted.");
                await Utility.RunOnUIThread(() =>
                {
                    var syncToRemove = _viewModel.AllSyncs.FirstOrDefault(s => s.DbId == syncDbId);
                    if (syncToRemove is not null)
                        syncToRemove.Drive.Syncs.Remove(syncToRemove);
                    Logger.Log(Logger.Level.Info, $"Sync with DbId {syncDbId} removed.");
                }).ConfigureAwait(false);
                return true;
            }

            if (!CheckJobResultAndLogIfError(commData, parms))
                return false;

            // Rely on signal to remove the sync from the model
            return true;
        }

        public async Task<bool> StartSync(DbId syncDbId, CancellationToken cancellationToken)
        {
            Sync? sync = _viewModel.AllSyncs.FirstOrDefault(s => s.DbId == syncDbId);
            if (sync is null)
            {
                Logger.Log(Logger.Level.Error, $"Sync with DbId {syncDbId} not found in model.");
                return false;
            }
            var previousStatus = sync.SyncStatus;
            sync.SyncStatus = SyncStatus.Starting;

            var parms = new JsonObject
            {
                [JsonKeys.SyncDbId] = syncDbId
            };
            CommData data = await _commClient.SendRequestAsync(RequestNum.SYNC_START, parms, cancellationToken).ConfigureAwait(false);
            if (!CheckJobResultAndLogIfError(data, parms))
            {
                sync.SyncStatus = previousStatus;
                return false;
            }

            return true;
        }

        public async Task<bool> PauseSync(DbId syncDbId, CancellationToken cancellationToken)
        {
            Sync? sync = _viewModel.AllSyncs.FirstOrDefault(s => s.DbId == syncDbId);
            if (sync is null)
            {
                Logger.Log(Logger.Level.Error, $"Sync with DbId {syncDbId} not found in model.");
                return false;
            }
            var previousStatus = sync.SyncStatus;
            sync.SyncStatus = SyncStatus.StopAsked;

            var parms = new JsonObject
            {
                [JsonKeys.SyncDbId] = syncDbId
            };
            CommData data = await _commClient.SendRequestAsync(RequestNum.SYNC_STOP, parms, cancellationToken).ConfigureAwait(false);
            if (!CheckJobResultAndLogIfError(data, parms))
            {
                sync.SyncStatus = previousStatus;
                return false;
            }
            return true;
        }

        public async Task<bool?> CanPathSupportLiteSync(string absoluteLocalPath, CancellationToken cancellationToken)
        {
            var parms = new JsonObject
            {
                [JsonKeys.Path] = Utility.ToBase64String(absoluteLocalPath)
            };

            CommData data = await _commClient.SendRequestAsync(RequestNum.UTILITY_BESTVFSAVAILABLEMODE, parms, cancellationToken).ConfigureAwait(false);
            if (!CheckJobResultAndLogIfError(data, parms))
                return null;

            if (!HasRequiredParam(data, JsonKeys.BestMode))
                return null;

            VirtualFileMode? bestMode = (VirtualFileMode?)(data.Params[JsonKeys.BestMode]?.GetValue<int>());
            if (!bestMode.HasValue)
            {
                Logger.Log(Logger.Level.Error, $"Failed to parse {JsonKeys.BestMode} from response: {data.Params}");
                return null;
            }

            return bestMode == VirtualFileMode.Win;
        }

        public async Task<string?> GetGoodPathForNewSync(IDrive? drive, string desiredPath, CancellationToken cancellationToken)
        {
            var parms = new JsonObject
            {
                [JsonKeys.BasePath] = Utility.ToBase64String(desiredPath)
            };

            CommData data = await _commClient.SendRequestAsync(RequestNum.UTILITY_FINDGOODPATHFORNEWSYNC, parms, cancellationToken).ConfigureAwait(false);
            if (!CheckJobResultAndLogIfError(data, parms))
                return null;

            if (!HasRequiredParam(data, JsonKeys.GoodPath) || !HasRequiredParam(data, JsonKeys.ErrorMessage))
                return null;

            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());

            string? result;
            result = data.Params[JsonKeys.GoodPath].Deserialize<string>(options);

            if (result is null)
            {
                Logger.Log(Logger.Level.Error, $"Failed to deserialize {JsonKeys.GoodPath} from response: {data.Params}");
                return null;
            }

            return result;
        }
        public async Task<bool?> IsPathValidForNewSync(string path, SyncConfiguration syncConfiguration, CancellationToken cancellationToken)
        {
            var parms = new JsonObject
            {
                [JsonKeys.Path] = Utility.ToBase64String(path),
                [JsonKeys.SyncConfiguration] = (int)syncConfiguration
            };

            CommData data = await _commClient.SendRequestAsync(RequestNum.UTILITY_ISPATHVALIDFORNEWSYNC, parms, cancellationToken);
            if (!CheckJobResultAndLogIfError(data, parms))
                return null;

            if (!HasRequiredParam(data, JsonKeys.IsValid))
                return null;

            bool? result = data.Params[JsonKeys.IsValid]?.GetValue<bool>();
            if (result is null)
            {
                Logger.Log(Logger.Level.Error, $"Failed to parse {JsonKeys.IsValid} from response: {data.Params}");
                return null;
            }

            return result;
        }

        public async Task<List<SearchItem>?> SearchItem(Sync? sync, string searchString, CancellationToken cancellationToken)
        {
            if (sync is null)
            {
                Logger.Log(Logger.Level.Error, "Sync is null.");
                return null;
            }

            var parms = new JsonObject
            {
                [JsonKeys.SyncDbId] = sync.DbId,
                [JsonKeys.SearchString] = Utility.ToBase64String(searchString)
            };

            CommData data = await _commClient.SendRequestAsync(RequestNum.DRIVE_SEARCH, parms, cancellationToken).ConfigureAwait(false);
            if (!CheckJobResultAndLogIfError(data, parms))
                return null;

            if (!HasRequiredParam(data, JsonKeys.SearchInfoList))
                return null;

            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            options.Converters.Add(new IntToDateTimeConverter());
            List<SearchInfo>? resultInfos = data.Params[JsonKeys.SearchInfoList]?.Deserialize<List<SearchInfo>>(options);

            if (resultInfos is null)
            {
                Logger.Log(Logger.Level.Error, $"Failed to deserialize SearchInfo list from ${data.Params[JsonKeys.SearchInfoList]}.");
                return null;
            }

            var resultItems = new List<SearchItem>();

            foreach (var item in resultInfos)
            {
                if (typeof(SearchInfo).GetProperties().Any(p => p.GetValue(item) is null))
                {
                    Logger.Log(Logger.Level.Error, $"SearchInfo contains null properties for item with NodeId {item.Id}. Skipping this item.");
                    continue;
                }

                resultItems.Add(new SearchItem(
                    item.Id!,
                    item.Name!,
                    item.Type!.Value,
                    item.Path!,
                    System.IO.Path.Combine(sync.Drive.Name, item.Path!),
                    item.ModifiedTime!.Value,
                    item.Size!.Value,
                    item.IsAvailableLocally!.Value
                ));
            }
            return resultItems;
        }
        public async Task<UInt64?> GetSyncOfflineFilesSize(DbId syncDbId, CancellationToken cancellationToken)
        {
            var parms = new JsonObject
            {
                [JsonKeys.SyncDbId] = syncDbId
            };

            CommData data = await _commClient.SendRequestAsync(RequestNum.SYNC_OFFLINE_FILES_SIZE, parms, cancellationToken).ConfigureAwait(false);
            if (!CheckJobResultAndLogIfError(data, parms))
                return null;

            if (!HasRequiredParam(data, JsonKeys.Size))
                return null;

            UInt64? size = data.Params[JsonKeys.Size]?.GetValue<UInt64>();
            if (!size.HasValue)
            {
                Logger.Log(Logger.Level.Error, $"Failed to parse {JsonKeys.Size} from response: {data.Params}");
                return null;
            }
            return size;
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
            CommData data = await _commClient.SendRequestAsync(RequestNum.NODE_SUBFOLDERS, parms, cancellationToken).ConfigureAwait(false);
            if (!CheckJobResultAndLogIfError(data, parms))
                return null;

            if (!HasRequiredParam(data, JsonKeys.NodeSubFolderInfoList))
                return null;

            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            List<NodeInfo>? nodeList = data.Params[JsonKeys.NodeSubFolderInfoList].Deserialize<List<NodeInfo>>(options);
            if (nodeList is null)
            {
                Logger.Log(Logger.Level.Error, $"Failed to deserialize nodeList from ${data.Params[JsonKeys.NodeSubFolderInfoList]}.");
                return null;
            }

            List<Node> nodes = nodeList.Select(n =>
            {
                return new Node(n.NodeId ?? "", n.Name ?? "", n.Size ?? 0, n.ParentNodeId ?? "", n.Path ?? "", userDbId, driveId, n.AccessDenied ?? false);
            }).ToList();

            return nodes;
        }
        public async Task<GetNodeInfoResult> GetNodeInfo(DbId userDbId, DriveId driveId, NodeId nodeId, CancellationToken cancellationToken)
        {
            var parms = new JsonObject
            {
                [JsonKeys.UserDbId] = userDbId,
                [JsonKeys.DriveId] = driveId,
                [JsonKeys.NodeId] = Utility.ToBase64String(nodeId),
                [JsonKeys.WithPath] = true
            };
            CommData data = await _commClient.SendRequestAsync(RequestNum.NODE_INFO, parms, cancellationToken).ConfigureAwait(false);
            if (!CheckJobResultAndLogIfError(data, parms))
                return new GetNodeInfoResult(data.Cause, null);

            if (!HasRequiredParam(data, JsonKeys.NodeInfo))
                return new GetNodeInfoResult(data.Cause, null);

            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            NodeInfo? nodeInfo = data.Params[JsonKeys.NodeInfo].Deserialize<NodeInfo>(options);
            if (nodeInfo is null)
            {
                Logger.Log(Logger.Level.Error, $"Failed to deserialize nodeInfo from ${data.Params[JsonKeys.NodeInfo]}.");
                return new GetNodeInfoResult(data.Cause, null);
            }
            return new GetNodeInfoResult(data.Cause, new Node(nodeInfo.NodeId ?? "", nodeInfo.Name ?? "", nodeInfo.Size ?? 0, nodeInfo.ParentNodeId ?? "", nodeInfo.Path ?? "", userDbId, driveId, nodeInfo?.AccessDenied ?? false));
        }

        public async Task<Int64?> GetFolderSize(long userDbId, long driveId, string nodeId, CancellationToken cancellationToken)
        {
            var parms = new JsonObject
            {
                [JsonKeys.UserDbId] = userDbId,
                [JsonKeys.DriveId] = driveId,
                [JsonKeys.NodeId] = Utility.ToBase64String(nodeId),
            };
            CommData data = await _commClient.SendRequestAsync(RequestNum.NODE_FOLDER_SIZE, parms, cancellationToken).ConfigureAwait(false);
            if (!CheckJobResultAndLogIfError(data, parms))
                return null;

            if (!HasRequiredParam(data, JsonKeys.FolderSize))
                return null;

            Int64? folderSize = data.Params[JsonKeys.FolderSize]?.GetValue<Int64>();
            if (!folderSize.HasValue)
            {
                Logger.Log(Logger.Level.Error, $"Failed to parse {JsonKeys.FolderSize} from response: {data.Params}");
                return null;
            }

            return folderSize;
        }

        public async Task<List<NodeId>?> GetBlacklistedNodeIdList(DbId syncDbId, CancellationToken cancellationToken)
        {
            var parms = new JsonObject
            {
                [JsonKeys.SyncDbId] = syncDbId,
            };
            CommData data = await _commClient.SendRequestAsync(RequestNum.BLACKLISTED_NODE_LIST, parms, cancellationToken).ConfigureAwait(false);
            if (!CheckJobResultAndLogIfError(data, parms))
                return null;

            if (!HasRequiredParam(data, JsonKeys.NodeIdList))
                return null;

            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            List<NodeId>? result = data.Params[JsonKeys.NodeIdList].Deserialize<List<NodeId>>(options);
            if (result is null)
            {
                Logger.Log(Logger.Level.Error, $"Failed to parse {JsonKeys.NodeIdList} from response: {data.Params}");
                return null;
            }
            return result;
        }

        public async Task<bool> SetBlacklistedNodeIdList(DbId syncDbId, List<NodeId> idList, CancellationToken cancellationToken)
        {
            var parms = new JsonObject
            {
                [JsonKeys.SyncDbId] = syncDbId,
                [JsonKeys.NodeIdList] = JsonSerializer.SerializeToNode(idList, new JsonSerializerOptions { Converters = { new Base64StringJsonConverter() } })
            };
            CommData data = await _commClient.SendRequestAsync(RequestNum.BLACKLISTED_NODE_SETLIST, parms, cancellationToken).ConfigureAwait(false);
            return CheckJobResultAndLogIfError(data, parms);
        }

        public async Task<NodeId?> CreateMissingDirectories(IDrive drive, NodeId parentNodeId, string path, CancellationToken cancellationToken)
        {
            if (string.IsNullOrEmpty(parentNodeId))
                parentNodeId = App.Constants.Sync.RootNodeId;

            var parms = new JsonObject
            {
                [JsonKeys.UserDbId] = drive.UserDbId,
                [JsonKeys.DriveId] = drive.DriveId,
                [JsonKeys.ParentNodeId] = Utility.ToBase64String(parentNodeId),
                [JsonKeys.RelativePath] = Utility.ToBase64String(path),

            };
            CommData data = await _commClient.SendRequestAsync(RequestNum.NODE_CREATEMISSINGFOLDERS, parms, cancellationToken).ConfigureAwait(false);
            if (!CheckJobResultAndLogIfError(data, parms))
                return null;

            if (!HasRequiredParam(data, JsonKeys.NodeId))
                return null;

            var options = new JsonSerializerOptions();
            options.Converters.Add(new Base64StringJsonConverter());
            NodeId? nodeId = data.Params[JsonKeys.NodeId].Deserialize<NodeId>(options);
            if (string.IsNullOrEmpty(nodeId))
            {
                Logger.Log(Logger.Level.Error, $"Failed to deserialize {JsonKeys.NodeId} from response: {data.Params}");
                return null;
            }

            return nodeId;
        }

        public async Task<Uri?> GetPublicLink(DbId driveDbId, NodeId nodeId, CancellationToken cancellationToken)
        {
            var parms = new JsonObject
            {
                [JsonKeys.DriveDbId] = driveDbId,
                [JsonKeys.NodeId] = Utility.ToBase64String(nodeId)
            };
            CommData data = await _commClient.SendRequestAsync(RequestNum.SYNC_GETPUBLICLINKURL, parms, cancellationToken).ConfigureAwait(false);
            if (!CheckJobResultAndLogIfError(data, parms))
                return null;

            if (!HasRequiredParam(data, JsonKeys.LinkUrl))
                return null;

            var options = new JsonSerializerOptions();
            options.Converters.Add(new Base64StringJsonConverter());
            string? linkStr = data.Params[JsonKeys.LinkUrl].Deserialize<string>(options);
            if (linkStr is null)
            {
                Logger.Log(Logger.Level.Error, $"Failed to deserialize {JsonKeys.LinkUrl} from response: {data.Params}");
                return null;
            }

            try
            {
                return new Uri(linkStr);
            }
            catch (UriFormatException)
            {
                Logger.Log(Logger.Level.Error, $"Invalid URI format received: {linkStr}");
                return null;
            }
        }

        public async Task<NodeConflictInfo?> GetNodeConflictInfo(DbId syncDbId, string relativePath, ReplicaSide replicaSide, CancellationToken cancellationToken)
        {
            var parms = new JsonObject
            {
                [JsonKeys.SyncDbId] = syncDbId,
                [JsonKeys.RelativePath] = Utility.ToBase64String(relativePath),
                [JsonKeys.ReplicaSide] = (int)replicaSide
            };
            CommData data = await _commClient.SendRequestAsync(RequestNum.NODE_CONFLICT_INFO, parms, cancellationToken).ConfigureAwait(false);
            if (!CheckJobResultAndLogIfError(data, parms))
                return null;

            if (!HasRequiredParam(data, JsonKeys.NodeConflictInfo))
                return null;

            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            options.Converters.Add(new IntToDateTimeConverter());

            NodeConflictInfo? nodeVersionInfo = data.Params[JsonKeys.NodeConflictInfo].Deserialize<NodeConflictInfo>(options);
            if (nodeVersionInfo is null)
            {
                Logger.Log(Logger.Level.Error, $"Failed to deserialize nodeVersionInfo from {data.Params[JsonKeys.NodeConflictInfo]}.");
                return null;
            }

            return nodeVersionInfo;
        }

        public async Task<bool> StartUpdate(CancellationToken cancellationToken)
        {
            CommData data = await _commClient.SendRequestAsync(RequestNum.UPDATER_START_INSTALLER, [], cancellationToken).ConfigureAwait(false);
            return CheckJobResultAndLogIfError(data);
        }

        public async Task<bool> RefreshUpdaterVersionInfo(CancellationToken cancellationToken)
        {
            // First check the update state
            CommData data = await _commClient.SendRequestAsync(RequestNum.UPDATER_STATE, [], cancellationToken);
            if (!CheckJobResultAndLogIfError(data))
                return false;

            if (!HasRequiredParam(data, JsonKeys.UpdateState))
                return false;

            UpdateState? updateState = (UpdateState?)data.Params[JsonKeys.UpdateState]?.GetValue<int>();
            if (updateState is null)
            {
                Logger.Log(Logger.Level.Error, $"Failed to parse {JsonKeys.UpdateState} from response: {data.Params}");
                return false;
            }

            // UpdateState.NoUpdate means that the current setup does not allow for updates at all, so we should disable update functionality in the UI and clear any available update info.
            if (updateState == UpdateState.NoUpdate)
            {
                _viewModel.Settings.UpdateManager.UpdateEnabled = false;
                _viewModel.Settings.UpdateManager.AvailableUpdate = null;
                return true;
            }
            _viewModel.Settings.UpdateManager.UpdateEnabled = true;

            List<UpdateState> notReadyStates =
            [
                UpdateState.UpToDate,
                UpdateState.Available,
                UpdateState.Downloading,
                UpdateState.CheckError,
                UpdateState.DownloadError,
                UpdateState.UpdateError
            ];

            if (notReadyStates.Contains(updateState.Value))
            {
                _viewModel.Settings.UpdateManager.AvailableUpdate = null;
                return true;
            }

            if (updateState == UpdateState.Checking)
            {
                return true;
            }

            // If we are here, it means we are in a state where an update is available, we can fetch the version details.
            var channel = _viewModel.Settings.UpdateManager.CurrentChannel;
            var parms = new JsonObject
            {
                [JsonKeys.UpdateChannel] = (int)channel
            };
            CommData data2 = await _commClient.SendRequestAsync(RequestNum.UPDATER_VERSION_INFO, parms, cancellationToken);
            if (!CheckJobResultAndLogIfError(data2, parms))
                return false;

            if (!HasRequiredParam(data2, JsonKeys.VersionInfo))
                return false;

            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            AppVersion? versionInfo = data2.Params[JsonKeys.VersionInfo].Deserialize<AppVersion>(options);
            if (versionInfo is null || versionInfo.Tag == "")
            {
                Logger.Log(Logger.Level.Error, $"Failed to deserialize VersionInfo from ${data2.Params[JsonKeys.VersionInfo]}.");
                return false;
            }

            if (_viewModel.Settings.AppVersion.IsLowerThan(versionInfo))
            {
                _viewModel.Settings.UpdateManager.AvailableUpdate = versionInfo;
            }
            else
                _viewModel.Settings.UpdateManager.AvailableUpdate = null;

            return true;
        }

        public async Task<bool> StartLogUpload(bool includeArchivedLogs, CancellationToken cancellationToken)
        {
            var parms = new JsonObject
            {
                [JsonKeys.IncludeArchivedLogs] = includeArchivedLogs
            };
            CommData data = await _commClient.SendRequestAsync(RequestNum.UTILITY_SEND_LOG_TO_SUPPORT, parms, cancellationToken);
            return CheckJobResultAndLogIfError(data, parms);
        }

        public async Task<bool> CancelLogUpload(CancellationToken cancellationToken)
        {
            CommData data = await _commClient.SendRequestAsync(RequestNum.UTILITY_CANCEL_LOG_TO_SUPPORT, [], cancellationToken);
            return CheckJobResultAndLogIfError(data);
        }

        public async Task<bool> RefreshSettings(CancellationToken cancellationToken)
        {
            CommData data = await _commClient.SendRequestAsync(RequestNum.PARAMETERS_INFO, [], cancellationToken);
            if (!CheckJobResultAndLogIfError(data))
                return false;

            if (!HasRequiredParam(data, JsonKeys.ParmsInfo))
                return false;

            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            ParmsInfo? parametersInfo = data.Params[JsonKeys.ParmsInfo].Deserialize<ParmsInfo>(options);
            if (parametersInfo is null)
            {
                Logger.Log(Logger.Level.Error, $"Failed to deserialize parmsInfo from ${data.Params["parmsInfo"]}.");
                return false;
            }
            CommStruct.ConversionHelper.CopyToSettings(parametersInfo, _viewModel.Settings);
            return true;
        }

        public async Task<bool> ActivateLoadInfo(CancellationToken cancellationToken)
        {
            CommData data = await _commClient.SendRequestAsync(RequestNum.UTILITY_ACTIVATELOADINFO, [], cancellationToken);
            return CheckJobResultAndLogIfError(data);
        }

        public async Task Exit()
        {
            // Try and forget, no need to wait for response on exit
            await _commClient.SendRequestAsync(RequestNum.UTILITY_QUIT, [], CancellationToken.None);
        }

        public async Task<bool> SaveSettings(CancellationToken cancellationToken)
        {
            ParmsInfo ParmsInfo = new();
            CommStruct.ConversionHelper.CopyToParmsInfo(_viewModel.Settings, ParmsInfo);
            JsonObject parms = new()
            {
                [JsonKeys.ParmsInfo] = new JsonObject()
            };
            var options = new JsonSerializerOptions
            {
                PropertyNamingPolicy = JsonNamingPolicy.CamelCase
            };
            options.Converters.Add(new Base64StringJsonConverter());
            string ParmsInfoJson = JsonSerializer.Serialize(ParmsInfo, options);
            parms[JsonKeys.ParmsInfo] = JsonNode.Parse(ParmsInfoJson);

            if (parms[JsonKeys.ParmsInfo] is null)
            {
                Logger.Log(Logger.Level.Error, "Failed to serialize ParmsInfo for saving settings.");
                return false;
            }

            CommData data = await _commClient.SendRequestAsync(RequestNum.PARAMETERS_UPDATE, parms, cancellationToken);
            if (!CheckJobResultAndLogIfError(data, parms))
                return false;

            return true;
        }

        public async Task<List<ExclusionTemplate>?> GetExclusionTemplates(CancellationToken cancellationToken)
        {
            List<ExclusionTemplate> result = [];

            foreach (bool def in new bool[] { false, true })
            {
                JsonObject parms = new()
                {
                    [JsonKeys.Default] = def
                };
                CommData data = await _commClient.SendRequestAsync(RequestNum.EXCLTEMPL_GETLIST, parms, cancellationToken).ConfigureAwait(false);
                if (!CheckJobResultAndLogIfError(data, parms))
                    return null;

                if (!HasRequiredParam(data, JsonKeys.ExclusionTemplatesList))
                    return null;

                var options = new JsonSerializerOptions
                {
                    PropertyNameCaseInsensitive = true
                };
                options.Converters.Add(new Base64StringJsonConverter());
                List<ExclusionTemplateInfo>? exclusionTemplateInfos = data.Params[JsonKeys.ExclusionTemplatesList].Deserialize<List<ExclusionTemplateInfo>>(options);
                if (exclusionTemplateInfos is null)
                {
                    Logger.Log(Logger.Level.Error, $"Failed to deserialize ExclusionTemplatesList from ${data.Params[JsonKeys.ExclusionTemplatesList]}.");
                    return null;
                }
                foreach (var info in exclusionTemplateInfos)
                {
                    result.Add(CommStruct.ConversionHelper.FromExclusionTemplateInfo(info));
                }
            }
            return result;
        }

        public async Task<bool> SetUserExclusionTemplates(List<ExclusionTemplate> templates, CancellationToken cancellationToken)
        {
            JsonObject parms = new()
            {
                [JsonKeys.ExclusionTemplatesList] = JsonSerializer.SerializeToNode(templates, new JsonSerializerOptions { Converters = { new Base64StringJsonConverter() }, PropertyNamingPolicy = JsonNamingPolicy.CamelCase })
            };
            CommData data = await _commClient.SendRequestAsync(RequestNum.EXCLTEMPL_SETUSERLIST, parms, cancellationToken).ConfigureAwait(false);
            return CheckJobResultAndLogIfError(data, parms);
        }

        public async Task<bool> RefreshErrors(CancellationToken cancellationToken)
        {
            JsonObject parms = new()
            {
                [JsonKeys.Limit] = 1000
            };
            CommData data = await _commClient.SendRequestAsync(RequestNum.ERROR_INFOLIST, parms, cancellationToken).ConfigureAwait(false);
            if (!CheckJobResultAndLogIfError(data, parms))
                return false;

            if (!HasRequiredParam(data, JsonKeys.ErrorInfoList))
                return false;

            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            options.Converters.Add(new IntToDateTimeConverter());
            List<ErrorInfo>? errorInfos = data.Params[JsonKeys.ErrorInfoList].Deserialize<List<ErrorInfo>>(options);
            if (errorInfos is null)
            {
                Logger.Log(Logger.Level.Error, $"Failed to deserialize errorInfoList from ${data.Params[JsonKeys.ErrorInfoList]}.");
                return false;
            }

            await _viewModel.ClearAllErrorsAsync().ConfigureAwait(false);
            foreach (var errorInfo in errorInfos)
            {
                if (errorInfo.Level == ErrorLevel.Server)
                {
                    Error error = new(errorInfo);
                    await _viewModel.AddErrorAsync(error).ConfigureAwait(false);
                }
                else if (errorInfo.SyncDbId is not null)
                {
                    var sync = App.ServiceProvider.GetRequiredService<AppModel>().AllSyncs.FirstOrDefault(s => s.DbId == errorInfo.SyncDbId);

                    if (sync is not null)
                    {
                        Error error = new(sync, errorInfo);
                        await _viewModel.AddErrorAsync(error).ConfigureAwait(false);
                    }
                    else
                    {
                        Logger.Log(Logger.Level.Error, $"Error with DbId {errorInfo.DbId} references Sync with DbId {errorInfo.SyncDbId}, but it was not found among all Syncs.");
                    }
                }
                else
                {
                    Logger.Log(Logger.Level.Error, $"Error with DbId {errorInfo.DbId} has invalid SyncDbId {errorInfo.SyncDbId}.");
                }
            }
            return true;
        }

        public async Task<bool> DeleteError(DbId errorDbId, CancellationToken cancellationToken)
        {
            var parms = new JsonObject
            {
                [JsonKeys.ErrorDbId] = errorDbId
            };
            CommData data = await _commClient.SendRequestAsync(RequestNum.ERROR_DELETE, parms, cancellationToken).ConfigureAwait(false);
            if (data?.Code == ExitCode.InvalidOperation)
            {
                Logger.Log(Logger.Level.Info, $"Error with DbId {errorDbId} cannot be deleted as it is kept by the server.");
                return false;
            }
            return CheckJobResultAndLogIfError(data, parms);
        }

        public async Task<bool> ResolveConflicts(List<DbId> keepLocalErrorDbIds, List<DbId> keepRemoteErrorDbIds, CancellationToken cancellationToken)
        {
            var parms = new JsonObject
            {
                [JsonKeys.KeepLocalErrorDbIdList] = JsonSerializer.SerializeToNode(keepLocalErrorDbIds),
                [JsonKeys.KeepRemoteErrorDbIdList] = JsonSerializer.SerializeToNode(keepRemoteErrorDbIds)
            };
            CommData data = await _commClient.SendRequestAsync(RequestNum.ERROR_RESOLVE_CONFLICTS, parms, cancellationToken).ConfigureAwait(false);
            return CheckJobResultAndLogIfError(data, parms);
        }

        public async Task<bool> ResolveConflictsQuick(List<DbId> errorDbIds, ConflictResolutionStrategy strategy, CancellationToken cancellationToken)
        {
            var parms = new JsonObject
            {
                [JsonKeys.ErrorDbIdList] = JsonSerializer.SerializeToNode(errorDbIds),
                [JsonKeys.Strategy] = (int)strategy
            };
            CommData data = await _commClient.SendRequestAsync(RequestNum.ERROR_RESOLVE_CONFLICTS_QUICK, parms, cancellationToken).ConfigureAwait(false);
            return CheckJobResultAndLogIfError(data, parms);
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
                case SignalNum.UTILITY_ERROR_REMOVED:
                    await HandleErrorRemovedAsync(sender, args);
                    break;
                case SignalNum.UTILITY_LOG_UPLOAD_STATUS_UPDATED:
                    await HandleLogUploadProgressAsync(sender, args);
                    break;
                case SignalNum.UTILITY_SHOW_NOTIFICATION:
                    await HandleUtilityShowNotification(sender, args);
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

            if (args.SignalNum == SignalNum.SYNC_ADDED)
                _viewModel.SelectedSync = _viewModel.AllSyncs.FirstOrDefault(s => s?.DbId == syncInfo.DbId, _viewModel.SelectedSync);
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

            await Utility.RunOnUIThread(() =>
            {
                const int MaxActivities = 500;

                var activities = sync.SyncActivities;
                var operationId = fileItemInfo.OperationId;

                // Store the position of the last syncing item
                var lastSyncingItem = activities.LastOrDefault(a => a.Status == SyncFileStatus.Syncing);
                int destIndex = lastSyncingItem is null ? 0 : activities.IndexOf(lastSyncingItem);

                // Find existing item
                var existing = activities.FirstOrDefault(a => a.OperationId == operationId);

                if (existing is not null)
                {
                    // Ignore duplicate completion signals
                    if (existing.Status != SyncFileStatus.Syncing)
                    {
                        Logger.Log(Logger.Level.Warning, $"Received completion for already finished item. OperationId: {existing.OperationId}, Existing: {existing.Status}, New: {fileItemInfo.Status}");
                        return;
                    }

                    // Update state
                    CommStruct.ConversionHelper.CopyToSyncFileItem(fileItemInfo, existing);
                    existing.Timestamp = DateTime.Now;

                    Logger.Log(Logger.Level.Extended, $"Updated item {existing.OperationId} to {existing.Status}");

                    // Still syncing -> nothing more to do
                    if (existing.Status == SyncFileStatus.Syncing)
                        return;

                    // Move completed/errored item after syncing group
                    if (destIndex != activities.IndexOf(existing))
                    {
                        activities.Remove(existing);
                        activities.Insert(Math.Min(destIndex, activities.Count), existing);
                    }

                    return;
                }

                Func<SyncFileItemInfo, SyncFileItem, bool> shouldBeRemoved = (info, item) =>
                {
                    // We never want to remove an item that as sucessfully completed
                    if (item.Status == SyncFileStatus.Success)
                        return false;


                    if (Error.IsConflictUserResolvable(item.Conflict) && !sync.SyncErrors.Any(e => e.IsConflictUserResolvable() && e.Path == item.Path))
                        return true;

                    if (!string.IsNullOrEmpty(info.RemoteNodeId) && !string.IsNullOrEmpty(item.RemoteNodeId) && info.RemoteNodeId == item.RemoteNodeId && !Error.IsConflictUserResolvable(item.Conflict))
                        return true;

                    if (!string.IsNullOrEmpty(info.LocalNodeId) && !string.IsNullOrEmpty(item.LocalNodeId) && info.LocalNodeId == item.LocalNodeId)
                        return true;

                    if (!sync.SyncErrors.Any(e => e.Path == item.Path))
                        return true;

                    return false;
                };

                // Remove any item with the same remote or local node id wich is not syncing or done (ie errored)
                var toBeRemoved = activities.Where(a => shouldBeRemoved(fileItemInfo, a)).ToList();
                activities.RemoveMany(toBeRemoved);

                // Create new item
                var newItem = new SyncFileItem(sync, fileItemInfo);

                const int MinFileSizeForTopSticking = 1024; // Don't stick items smaller than 1KB to the top as they are likely to complete very fast, otherwise we might end up with flickering in the UI with items jumping from the top to the middle of the list when they are completed.
                if (newItem.Status != SyncFileStatus.Syncing || newItem.Size < MinFileSizeForTopSticking)
                {
                    // Insert item after all syncing items
                    activities.Insert(Math.Clamp(destIndex, 0, activities.Count), newItem);
                }
                else
                {
                    activities.Insert(0, newItem);
                }

                // Enforce max size
                while (activities.Count > MaxActivities)
                    activities.RemoveAt(activities.Count - 1);
            });
        }

        public async Task HandleUpdaterStateChangedAsync(object? sender, SignalEventArgs args)
        {
            await RefreshUpdaterVersionInfo(new CancellationToken());
        }

        public async Task HandleLogUploadProgressAsync(object? sender, SignalEventArgs args)
        {
            var signalData = args.SignalData;
            if (signalData == null || !signalData.ContainsKey(JsonKeys.State) || !signalData.ContainsKey(JsonKeys.Percentage))
            {
                Logger.Log(Logger.Level.Error, $"{JsonKeys.State} or {JsonKeys.Percentage} not found in parameters ${signalData}.");
                return;
            }
            LogUploadState? state = signalData[JsonKeys.State]?.Deserialize<LogUploadState>();
            int? percentage = signalData[JsonKeys.Percentage]?.GetValue<int>();
            if (state is null)
            {
                Logger.Log(Logger.Level.Error, "state is null.");
                return;
            }
            if (percentage is null)
            {
                Logger.Log(Logger.Level.Error, "percentage is null.");
                return;
            }
            _viewModel.Settings.LogUploadManager.State = state ?? LogUploadState.Failed;
            _viewModel.Settings.LogUploadManager.PercentComplete = percentage ?? 0;
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
            options.Converters.Add(new IntToDateTimeConverter());
            ErrorInfo? errorInfo = signalData[JsonKeys.ErrorInfo]?.Deserialize<ErrorInfo>(options);
            if (errorInfo is null)
            {
                Logger.Log(Logger.Level.Error, $"Failed to deserialize errorInfo from {signalData[JsonKeys.ErrorInfo]}.");
                return;
            }

            var sync = App.ServiceProvider.GetRequiredService<AppModel>().AllSyncs.FirstOrDefault(s => s.DbId == errorInfo.SyncDbId);

            if (sync is not null)
            {
                Error error = new(sync, errorInfo);
                await _viewModel.AddErrorAsync(error).ConfigureAwait(false);
            }
            else
            {
                Logger.Log(Logger.Level.Error, $"Error with DbId {errorInfo.DbId} references Sync with DbId {errorInfo.SyncDbId}, but it was not found among all Syncs.");
            }
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
            await _viewModel.RemoveErrorByDbIdAsync(errorDbId.Value);
        }

        public async Task HandleUtilityShowNotification(object? sender, SignalEventArgs args)
        {
            var signalData = args.SignalData;
            if (signalData == null || !signalData.ContainsKey(JsonKeys.Title) || !signalData.ContainsKey(JsonKeys.Message))
            {
                Logger.Log(Logger.Level.Error, $"{JsonKeys.Title} or {JsonKeys.Message} not found in parameters.");
                return;
            }
            string? title = signalData[JsonKeys.Title]?.GetValue<string>();
            string? message = signalData[JsonKeys.Message]?.GetValue<string>();

            // Decode base64 encoded 
            try
            {
                title = title is not null ? Encoding.UTF8.GetString(Convert.FromBase64String(title)) : null;
                message = message is not null ? Encoding.UTF8.GetString(Convert.FromBase64String(message)) : null;
            }
            catch (FormatException ex)
            {
                Logger.Log(Logger.Level.Error, $"Failed to decode title or message from base64. Title: {title}, Message: {message}. Exception: {ex}");
                return;
            }
            catch (Exception ex)
            {
                Logger.Log(Logger.Level.Error, $"Unexpected error while decoding title or message from base64. Title: {title}, Message: {message}. Exception: {ex}");
                return;
            }

            if (title is null || message is null)
            {
                Logger.Log(Logger.Level.Error, "title or message is null.");
                return;
            }

            App.ServiceProvider.GetRequiredService<NotificationManager>().ShowNotification(title, message);
        }

        // Helpers
        private async Task AddOrUpdateUserInModel(UserInfo userInfo)
        {

            if (!userInfo.DbId.HasValue)
            {
                Logger.Log(Logger.Level.Error, "userInfo.DbId is null.");
                return;
            }
            DbId dbId = userInfo.DbId.Value;

            User? user = _viewModel.Users.FirstOrDefault(u => u?.DbId == dbId, null);
            if (user is not null)
            {
                Logger.Log(Logger.Level.Extended, $"User with DbId {dbId} already exists in the application, updating...");
                ConversionHelper.CopyToUser(userInfo, user);
                Logger.Log(Logger.Level.Info, $"User with DbId {dbId} updated.");
            }
            else
            {
                User newUser = new User(dbId);
                ConversionHelper.CopyToUser(userInfo, newUser);
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
                    ConversionHelper.CopyToAccount(accountInfo, account);
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
                ConversionHelper.CopyToAccount(accountInfo, newAccount);
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
                    ConversionHelper.CopyToDrive(driveInfo, drive);
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
                    ConversionHelper.CopyToDrive(driveInfo, newDrive);
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
                List<Drive> allDrives = [];
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
                    ConversionHelper.CopyToSync(syncInfo, sync);
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
                    ConversionHelper.CopyToSync(syncInfo, newSync);
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


