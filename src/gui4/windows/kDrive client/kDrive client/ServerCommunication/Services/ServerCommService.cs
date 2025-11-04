using Infomaniak.kDrive.ServerCommunication.CommStruct;
using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.ServerCommunication.JsonConverters;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using System;
using System.Collections.Generic;
using System.Linq;
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
            CommData data = await _commClient.SendRequestAsync(RequestNum.UserDbIds, new JsonObject(), cancellationToken);
            if (data.Params == null || !data.Params.ContainsKey("userDbIds"))
            {
                Logger.Log(Logger.Level.Error, "userDbIds not found in response.");
                return null;
            }
            return data.Params["userDbIds"].Deserialize<List<DbId>>();
        }

        public async Task<User?> AddOrRelogUser(string oAuthCode, string oAuthCodeVerifier, CancellationToken cancellationToken)
        {
            CommData data = await _commClient.SendRequestAsync(RequestNum.LoginRequestToken, new JsonObject
            {
                ["code"] = Convert.ToBase64String(Encoding.UTF8.GetBytes(oAuthCode)),
                ["codeVerifier"] = Convert.ToBase64String(Encoding.UTF8.GetBytes(oAuthCodeVerifier))
            }, cancellationToken);

            if (data.Params == null || !data.Params.ContainsKey("userDbId") || data.Params["userDbId"] == null)
            {
                Logger.Log(Logger.Level.Error, "userDbId not found in response.");
                return null;
            }

            DbId userDbId = data.Params["userDbId"]?.GetValue<DbId>() ?? -1;

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
            CommData data = await _commClient.SendRequestAsync(RequestNum.UserInfoList, new JsonObject(), cancellationToken);
            if (data.Params == null || !data.Params.ContainsKey("userInfoList"))
            {
                Logger.Log(Logger.Level.Error, "userInfoList not found in response.");
                return;
            }
            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            List<UserInfo> userInfos = data.Params["userInfoList"].Deserialize<List<UserInfo>>(options) ?? new List<UserInfo>();

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

        public async Task RefreshAccounts(CancellationToken cancellationToken)
        {
            CommData data = await _commClient.SendRequestAsync(RequestNum.AccountInfoList, new JsonObject(), cancellationToken);
            if (data.Params == null || !data.Params.ContainsKey("accountInfoList"))
            {
                Logger.Log(Logger.Level.Error, "accountInfoList not found in response.");
                return;
            }
            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            List<AccountInfo> accountInfos = data.Params["accountInfoList"].Deserialize<List<AccountInfo>>(options) ?? new List<AccountInfo>();

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
            CommData data = await _commClient.SendRequestAsync(RequestNum.DriveInfoList, new JsonObject(), cancellationToken);
            if (data.Params == null || !data.Params.ContainsKey("driveInfoList"))
            {
                Logger.Log(Logger.Level.Error, "driveInfoList not found in response.");
                return;
            }
            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            options.Converters.Add(new ColorJsonConverter());
            List<DriveInfo> driveInfos = data.Params["driveInfoList"].Deserialize<List<DriveInfo>>(options) ?? new List<DriveInfo>();

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
                ["userDbId"] = userDbId
            };
            CommData data = await _commClient.SendRequestAsync(RequestNum.USER_AVAILABLEDRIVES, parms, cancellationToken);
            if (data.Params == null || !data.Params.ContainsKey("driveAvailableInfoList"))
            {
                Logger.Log(Logger.Level.Error, "driveAvailableInfoList not found in response.");
                return;
            }
            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            options.Converters.Add(new ColorJsonConverter());
            List<DriveAvailableInfo> driveAvailableInfoList = data.Params["driveAvailableInfoList"].Deserialize<List<DriveAvailableInfo>>(options) ?? new List<DriveAvailableInfo>();
            User? user = _viewModel.Users.FirstOrDefault<User>(u => u.DbId == userDbId);
            if (user is null)
            {
                Logger.Log(Logger.Level.Error, $"User not found for DriveAvailable with dbID {userDbId}.");
                return;
            }
            await Utility.RunOnUIThread(() =>
            {
                user.DrivesAvailable.Clear();
                foreach (var driveAvailableInfos in driveAvailableInfoList)
                {
                    DriveAvailable driveAvailable = new DriveAvailable();
                    CommStruct.ConversionHelper.copyToDriveAvailable(driveAvailableInfos, driveAvailable);
                    // Only add drives that are not already present in one of the user's accounts
                    if (user.Drives.Any(d => d.DriveId == driveAvailable.DriveId))
                        continue;
                    user.DrivesAvailable.Add(driveAvailable);
                }
            });
        }


        public async Task RefreshSyncs(CancellationToken cancellationToken)
        {
            CommData data = await _commClient.SendRequestAsync(RequestNum.SyncInfoList, new JsonObject(), cancellationToken);
            if (data.Params == null || !data.Params.ContainsKey("syncInfoList"))
            {
                Logger.Log(Logger.Level.Error, "syncInfoList not found in response.");
                return;
            }
            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            List<SyncInfo> syncInfos = data.Params["syncInfoList"].Deserialize<List<SyncInfo>>(options) ?? new List<SyncInfo>();

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

        public async Task RefreshSettings(CancellationToken cancellationToken)
        {
            CommData data = await _commClient.SendRequestAsync(RequestNum.PARAMETERS_INFO, new JsonObject(), cancellationToken);
            if (data.Params == null || !data.Params.ContainsKey("parmsInfo"))
            {
                Logger.Log(Logger.Level.Error, "parmsInfo not found in response.");
                return;
            }
            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            ParmsInfo? parametersInfo = data.Params["parmsInfo"].Deserialize<ParmsInfo>(options);
            if (parametersInfo == null)
            {
                Logger.Log(Logger.Level.Error, $"Failed to deserialize parmsInfo from ${data.Params["parmsInfo"]}.");
                return;
            }
            CommStruct.ConversionHelper.copyToSettings(parametersInfo, _viewModel.Settings);
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
                ["channel"] = (int)channel
            };
            CommData data = await _commClient.SendRequestAsync(RequestNum.UPDATER_VERSION_INFO, parms, cancellationToken);
            if (data.Params == null || !data.Params.ContainsKey("versionInfo"))
            {
                Logger.Log(Logger.Level.Error, "versionInfo not found in response.");
                return;
            }

            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            AppVersion? versionInfo = data.Params["versionInfo"].Deserialize<AppVersion>(options);
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
            await _commClient.SendRequestAsync(RequestNum.UPDATER_CHANGE_CHANNEL, new JsonObject { ["channel"] = (int)newChannel }, cancellationToken);
            _viewModel.Settings.UpdateManager.CurrentChannel = newChannel;
        }

        public async Task SaveSettings(CancellationToken cancellationToken)
        {
            ParmsInfo ParmsInfo = new();
            CommStruct.ConversionHelper.copyToParmsInfo(_viewModel.Settings, ParmsInfo);
            JsonObject parms = new()
            {
                ["parmsInfos"] = new JsonObject()
            };

            string ParmsInfoJson = JsonSerializer.Serialize(ParmsInfo);
            parms["parmsInfos"] = JsonNode.Parse(ParmsInfoJson) ?? new JsonObject();
            await _commClient.SendRequestAsync(RequestNum.PARAMETERS_UPDATE, parms, cancellationToken);
        }

        // Signals
        public async void OnSignalReceived(object? sender, SignalEventArgs args)
        {
            switch (args.SignalNum)
            {
                case SignalNum.UserUpdated:
                case SignalNum.UserAdded:
                    await HandleUserUpdatedOrAdded(sender, args);
                    break;
                case SignalNum.UPDATER_STATE_CHANGED:
                    await HandleUpdaterStateChanged(sender, args);
                    break;
                default:
                    Logger.Log(Logger.Level.Warning, $"Unhandled signal received: {args.SignalNum}");
                    break;
            }
        }

        public async Task HandleUserUpdatedOrAdded(object? sender, SignalEventArgs args)
        {
            var signalData = args.SignalData;

            if (signalData == null || !signalData.ContainsKey("userInfo"))
            {
                Logger.Log(Logger.Level.Error, "userInfo not found in parameters.");
                return;
            }

            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            UserInfo? newUserInfo = signalData["userInfo"].Deserialize<UserInfo>(options);
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

        public async Task HandleUpdaterStateChanged(object? sender, SignalEventArgs args)
        {
            await RefreshUpdaterVersionInfo(new CancellationToken());
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
            foreach (var user in _viewModel.Users)
            {
                if (user.Accounts.Any(a => a.DbId == accountInfo.DbId))
                {
                    Logger.Log(Logger.Level.Info, $"Account with DbId {accountInfo.DbId} already exists in the application.");
                    var account = user.Accounts.FirstOrDefault(a => a.DbId == accountInfo.DbId);
                    if (account == null)
                    {
                        Logger.Log(Logger.Level.Error, $"Unexpected error, account with DbId {accountInfo.DbId} not found after existence check.");
                        return;
                    }
                    ConversionHelper.copyToAccount(accountInfo, account);
                    Logger.Log(Logger.Level.Info, $"Account with DbId {accountInfo.DbId} updated.");
                    return;
                }
            }

            // Account not found, add it to the correct user
            DbId? userDbId = accountInfo.UserDbId; // Assuming AccountInfo has a UserDbId property
            var parentUser = _viewModel.Users.FirstOrDefault(u => u.DbId == userDbId);
            if (parentUser == null)
            {
                Logger.Log(Logger.Level.Error, $"Parent user with DbId {userDbId} not found for account DbId {accountInfo.DbId}.");
                return;
            }

            await Utility.RunOnUIThread(() =>
            {
                var newAccount = new Account(accountInfo.DbId ?? throw new InvalidOperationException("DbId should not be null here."), parentUser);
                ConversionHelper.copyToAccount(accountInfo, newAccount);
                parentUser.Accounts.Add(newAccount);
            });
            Logger.Log(Logger.Level.Info, $"New account added to user with DbId {userDbId}.");
        }

        private async Task AddOrUpdateDriveInModel(DriveInfo driveInfo)
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
                return;
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
                Logger.Log(Logger.Level.Error, $"Parent account with DbId {accountDbId} not found for drive DbId {driveInfo.DbId}.");
                return;
            }
            await Utility.RunOnUIThread(() =>
            {
                Drive newDrive = new Drive(driveInfo.DbId ?? throw new InvalidOperationException("DbId should not be null here."), parentAccount);
                ConversionHelper.copyToDrive(driveInfo, newDrive);
                parentAccount.Drives.Add(newDrive);

            });
            Logger.Log(Logger.Level.Info, $"New drive added to account with DbId {accountDbId}.");
        }

        private async Task AddOrUpdateSyncInModel(SyncInfo syncInfo)
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
                return;
            }
            // Sync not found, add it to the correct drive
            DbId? driveDbId = syncInfo.DriveDbId; // Assuming SyncInfo has a DriveDbId property
            var parentDrive = allDrives.FirstOrDefault(d => d.DbId == driveDbId);
            if (parentDrive == null)
            {
                Logger.Log(Logger.Level.Error, $"Parent drive with DbId {driveDbId} not found for sync DbId {syncInfo.DbId}.");
                return;
            }
            await Utility.RunOnUIThread(() =>
            {
                Sync newSync = new Sync(syncInfo.DbId ?? throw new InvalidOperationException("DbId should not be null here."), parentDrive);
                ConversionHelper.copyToSync(syncInfo, newSync);
                parentDrive.Syncs.Add(newSync);
            });
            Logger.Log(Logger.Level.Info, $"New sync added to drive with DbId {driveDbId}.");
        }
    }
}


