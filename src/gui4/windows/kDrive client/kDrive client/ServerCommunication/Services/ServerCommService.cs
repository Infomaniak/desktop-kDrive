using CommunityToolkit.WinUI;
using DynamicData;
using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.ServerCommunication.JsonConverters;
using Infomaniak.kDrive.ServerCommunication.MockData;
using Infomaniak.kDrive.ViewModels;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;
using static Infomaniak.kDrive.ServerCommunication.CommShared;
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
            CommData data = await _commClient.SendRequestAsync(CommShared.RequestNum.UserDbIds, new JsonObject(), cancellationToken);
            if (data.Params == null || !data.Params.ContainsKey("userDbIds"))
            {
                Logger.Log(Logger.Level.Error, "userDbIds not found in response.");
                return null;
            }
            return data.Params["userDbIds"].Deserialize<List<DbId>>();
        }

        public async Task<User?> AddOrRelogUser(string oAuthCode, string oAuthCodeVerifier, CancellationToken cancellationToken)
        {
            CommData data = await _commClient.SendRequestAsync(CommShared.RequestNum.LoginRequestToken, new JsonObject
            {
                ["code"] = Convert.ToBase64String(Encoding.UTF8.GetBytes(oAuthCode)),
                ["codeVerifier"] = Convert.ToBase64String(Encoding.UTF8.GetBytes(oAuthCodeVerifier))
            }, cancellationToken);

            if (data.Params == null || !data.Params.ContainsKey("userDbId") || data.Params["userDbId"] == null)
            {
                Logger.Log(Logger.Level.Error, "userDbId not found in response.");
                return null;
            }

            int userDbId = data.Params["userDbId"]?.GetValue<int>() ?? -1;

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
            return _viewModel.Users.FirstOrDefault(u => u.DbId == userDbId, null);
        }

        public async Task RefreshUsers(CancellationToken cancellationToken)
        {
            CommData data = await _commClient.SendRequestAsync(CommShared.RequestNum.UserInfoList, new JsonObject(), cancellationToken);
            if (data.Params == null || !data.Params.ContainsKey("userInfo"))
            {
                Logger.Log(Logger.Level.Error, "userInfo not found in response.");
                return;
            }
            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            List<UserInfo> userInfos = data.Params["userInfo"].Deserialize<List<UserInfo>>(options) ?? new List<UserInfo>();

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
            CommData data = await _commClient.SendRequestAsync(CommShared.RequestNum.AccountInfoList, new JsonObject(), cancellationToken);
            if (data.Params == null || !data.Params.ContainsKey("accountInfo"))
            {
                Logger.Log(Logger.Level.Error, "accountInfo not found in response.");
                return;
            }
            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            List<AccountInfo> accountInfos = data.Params["accountInfo"].Deserialize<List<AccountInfo>>(options) ?? new List<AccountInfo>();

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
            CommData data = await _commClient.SendRequestAsync(CommShared.RequestNum.DriveInfoList, new JsonObject(), cancellationToken);
            if (data.Params == null || !data.Params.ContainsKey("driveInfo"))
            {
                Logger.Log(Logger.Level.Error, "driveInfo not found in response.");
                return;
            }
            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            List<DriveInfo> driveInfos = data.Params["driveInfo"].Deserialize<List<DriveInfo>>(options) ?? new List<DriveInfo>();

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

        public async Task RefreshSyncs(CancellationToken cancellationToken)
        {
            CommData data = await _commClient.SendRequestAsync(CommShared.RequestNum.SyncInfoList, new JsonObject(), cancellationToken);
            if (data.Params == null || !data.Params.ContainsKey("syncInfo"))
            {
                Logger.Log(Logger.Level.Error, "syncInfo not found in response.");
                return;
            }
            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());
            List<SyncInfo> syncInfos = data.Params["syncInfo"].Deserialize<List<SyncInfo>>(options) ?? new List<SyncInfo>();

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

        // Signals
        public void OnSignalReceived(object? sender, SignalEventArgs args)
        {
            switch (args.SignalNum)
            {
                case SignalNum.UserUpdated:
                case SignalNum.UserAdded:
                    HandleUserUpdatedOrAddedAsync(sender, args);
                    break;
                default:
                    Logger.Log(Logger.Level.Warning, $"Unhandled signal received: {args.SignalNum}");
                    break;
            }
        }

        public void HandleUserUpdatedOrAddedAsync(object? sender, SignalEventArgs args)
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
            UserInfo newUserInfo = signalData["userInfo"].Deserialize<UserInfo>(options);
            if (newUserInfo.DbId is null)
            {
                Logger.Log(Logger.Level.Error, "userInfo.DbId is null.");
                return;
            }
            AddOrUpdateUserInModel(newUserInfo).GetAwaiter().GetResult();
        }


        // Helpers
        static void CopyProperties<T, V>(T target, V source, HashSet<string>? excludedProperties = null, Dictionary<string /*sourceName*/ , string /*TargetName*/ >? propertyMappings = null)
        {
            // Converters
            Dictionary<Type, Dictionary<Type, Func<object, object>>> converters = new Dictionary<Type, Dictionary<Type, Func<object, object>>>
            {
                { typeof(Int32), new Dictionary<Type, Func<object, object>>
                    {
                        { typeof(System.Drawing.Color), (obj) => System.Drawing.Color.FromArgb(System.Convert.ToInt32(obj)) }
                    }
                }
            };

            var sourceProperties = typeof(V).GetProperties().Where(p => p.CanRead && (excludedProperties == null || !excludedProperties.Contains(p.Name)));
            foreach (var sourceProp in sourceProperties)
            {
                var sourceValue = sourceProp.GetValue(source);
                if (sourceValue == null)
                    continue;

                string targetPropName = "";
                if (propertyMappings is not null && propertyMappings.ContainsKey(sourceProp.Name))
                {
                    targetPropName = propertyMappings[sourceProp.Name];
                    Logger.Log(Logger.Level.Extended, $"Mapping property {sourceProp.Name} from {typeof(V).Name} to {targetPropName} in {typeof(T).Name}");
                }
                else
                {
                    targetPropName = sourceProp.Name;
                }
                var targetProp = typeof(T).GetProperty(targetPropName);
                if (targetProp != null && targetProp.CanWrite)
                {
                    var targetValueType = Nullable.GetUnderlyingType(targetProp.PropertyType) ?? targetProp.PropertyType;
                    var sourceValueType = Nullable.GetUnderlyingType(sourceProp.PropertyType) ?? sourceProp.PropertyType;
                    if (targetValueType != sourceValueType)
                    {
                        if (converters.ContainsKey(sourceValueType) && converters[sourceValueType].ContainsKey(targetValueType))
                        {
                            sourceValue = converters[sourceValueType][targetValueType](sourceValue);
                        }
                        else
                        {
                            Logger.Log(Logger.Level.Warning, $"No converter found for property {sourceProp.Name} from type {sourceValueType.Name} to {targetValueType.Name}");
                            continue;
                        }
                    }

                    var targetValue = targetProp.GetValue(target);
                    if (!Equals(sourceValue, targetValue))
                        targetProp.SetValue(target, sourceValue);
                }
                else
                {
                    Logger.Log(Logger.Level.Warning, $"Property {sourceProp.Name} from source type {typeof(V).Name} not found or not writable in target type {typeof(T).Name}");
                }
            }
        }

        static void CopyUserProperties(User target, UserInfo source)
        {
            CopyProperties(target, source, null);
        }

        static void CopyAccountProperties(Account target, AccountInfo source)
        {
            var excludedProperties = new HashSet<string> { "UserDbId" };
            CopyProperties(target, source, excludedProperties);
        }

        static void CopyDriveProperties(Drive target, DriveInfo source)
        {
            var excludedProperties = new HashSet<string> { "AccountDbId", "Notification", "Maintenance", "Locked", "AccessDenied" };
            CopyProperties(target, source, excludedProperties);
        }

        static void CopySyncProperties(Sync target, SyncInfo source)
        {
            var excludedProperties = new HashSet<string> { "DriveDbId", "TargetNodeId", "SupportOnlineMode", "OnlineMode" };
            var propertyMappings = new Dictionary<string, string>
            {
                { "TargetPath", "RemotePath" }
            };
            CopyProperties(target, source, excludedProperties, propertyMappings);
        }

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
                CopyUserProperties(user, userInfo);
                Logger.Log(Logger.Level.Info, $"User with DbId {userInfo.DbId} updated.");
            }
            else
            {
                User newUser = new User(userInfo.DbId ?? throw new InvalidOperationException("DbId should not be null here."));
                CopyUserProperties(newUser, userInfo);
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
                    CopyAccountProperties(account, accountInfo);
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
                CopyAccountProperties(newAccount, accountInfo);
                parentUser.Accounts.Add(newAccount);
            });
            Logger.Log(Logger.Level.Info, $"New account added to user with DbId {userDbId}.");
        }

        private async Task AddOrUpdateDriveInModel(DriveInfo driveInfo)
        {
            foreach (var user in _viewModel.Users)
            {

                var drive = user.Drives.FirstOrDefault(d => d.DbId == driveInfo.DbId, null);
                if (drive == null)
                {
                    continue;
                }
                CopyDriveProperties(drive, driveInfo);
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
                CopyDriveProperties(newDrive, driveInfo);
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
                var sync = drive.Syncs.FirstOrDefault(s => s.DbId == syncInfo.DbId, null);
                if (sync == null)
                {
                    continue;
                }
                CopySyncProperties(sync, syncInfo);
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
                CopySyncProperties(newSync, syncInfo);
                parentDrive.Syncs.Add(newSync);
            });
            Logger.Log(Logger.Level.Info, $"New sync added to drive with DbId {driveDbId}.");
        }
    }
}


