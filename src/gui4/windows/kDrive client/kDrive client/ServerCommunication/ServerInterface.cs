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

using CommunityToolkit.WinUI;
using Infomaniak.kDrive.ServerCommunication.JsonConverters;
using Infomaniak.kDrive.ServerCommunication.MockData;
using Infomaniak.kDrive.ServerCommunication.Services;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Media;
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using System.Xml.Linq;

namespace Infomaniak.kDrive.ServerCommunication
{
    public class ServerInterface
    {
        static private MockData.MockServerData _mockServerData = new MockServerData();
        public AppModel ViewModel { get; private set; }
        private SocketServerCommProtocol CommClient { get; set; } = new SocketServerCommProtocol();


        public ServerInterface(AppModel appModel)
        {
            ViewModel = appModel;
        }

        public void Initialize()
        {
            CommClient.Initialize();
        }

        // User-related requests
        public async Task<DbId> AddOrRelogUser(string oAuthCode, string oAuthCodeVerifier, CancellationToken canceletionToken)
        {


            return Convert.ToInt32(-1);
        }

        Task OnUserAddedorUpdated(Dictionary<string, object>? parameters)
        {
            if (parameters == null || !parameters.ContainsKey("userInfo"))
            {
                Logger.Log(Logger.Level.Error, "OnUserAdded: userDbId not found in parameters.");
                return Task.CompletedTask;
            }

            string userInfoJson = parameters["userInfo"].ToString() ?? "";
            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };
            options.Converters.Add(new Base64StringJsonConverter());

            User? newUser = JsonSerializer.Deserialize<User>(userInfoJson, options);
            if (newUser == null)
            {
                Logger.Log(Logger.Level.Error, "OnUserAdded: Failed to deserialize userInfo.");
                return Task.CompletedTask;
            }

            AppModel.UIThreadDispatcher.TryEnqueue(() =>
            {
                if (ViewModel.Users.Any(u => u.DbId == newUser.DbId))
                {
                    Logger.Log(Logger.Level.Info, $"User with DbId {newUser.DbId} already exists in the application.");
                    var user = ViewModel.Users.FirstOrDefault(u => u.DbId == newUser.DbId);
                    user.UserId = newUser.UserId;
                    user.Name = newUser.Name;
                    user.Email = newUser.Email;
                    user.Avatar = newUser.Avatar;
                    user.IsConnected = newUser.IsConnected;
                    user.IsStaff = newUser.IsStaff;
                }
                else
                {
                    ViewModel.Users.Add(newUser);
                    Logger.Log(Logger.Level.Info, $"OnUserAdded: New user added with DbId {newUser.DbId}.");
                }
            });

            return Task.CompletedTask;
        }


        // The requests below are mock implementations simulating server responses. -- TODO: Replace with actual server communication logic.
        private static async Task SimulateNetworkDelay()
        {
            await Task.Delay(100); // Simulate network delay
        }

        public static async Task<List<DbId>> GetUserDbIds()
        {
            var users = _mockServerData.Users;
            await SimulateNetworkDelay();
            return users.Select(u => u.DbId).ToList();
        }

        public static async Task<UserId?> GetUserId(DbId dbId)
        {
            var users = _mockServerData.Users;
            await SimulateNetworkDelay();
            return users.FirstOrDefault(u => u.DbId == dbId)?.UserId;
        }

        public static async Task<string?> GetUserName(DbId dbId)
        {
            var users = _mockServerData.Users;
            await SimulateNetworkDelay();
            return users.FirstOrDefault(u => u.DbId == dbId)?.Name;
        }

        public static async Task<string?> GetUserEmail(DbId dbId)
        {
            var users = _mockServerData.Users;
            await SimulateNetworkDelay();
            return users.FirstOrDefault(u => u.DbId == dbId)?.Email;
        }

        public static async Task<ImageSource?> GetUserAvatar(DbId dbId)
        {
            //var users = await GetUsers().ConfigureAwait(false);
            var users = _mockServerData.Users;
            await SimulateNetworkDelay();
            return users.FirstOrDefault(u => u.DbId == dbId)?.AvatarImageSource;
        }

        public static async Task<bool?> GetUserIsConnected(DbId dbId)
        {
            var users = _mockServerData.Users;
            await SimulateNetworkDelay();
            return users.FirstOrDefault(u => u.DbId == dbId)?.IsConnected;
        }

        public static async Task<bool?> GetUserIsStaff(DbId dbId)
        {
            var users = _mockServerData.Users;
            await SimulateNetworkDelay();
            return users.FirstOrDefault(u => u.DbId == dbId)?.IsStaff;
        }

        public static async Task<List<DbId>> GetUserDrivesDbIds(DbId dbId)
        {
            var users = _mockServerData.Users;
            await SimulateNetworkDelay();
            return users.FirstOrDefault(u => u.DbId == dbId)?.Accounts.SelectMany(a => a.Drives).Select(d => d!.DbId).ToList() ?? new List<DbId>();
        }

        // Drive-related requests
        public static async Task<DriveId?> GetDriveId(DbId dbId)
        {
            var drives = _mockServerData.Drives;
            await SimulateNetworkDelay();
            return drives.FirstOrDefault(d => d.DbId == dbId)?.DriveId;
        }

        public static async Task<string?> GetDriveName(DbId dbId)
        {
            var drives = _mockServerData.Drives;
            await SimulateNetworkDelay();
            return drives.FirstOrDefault(d => d.DbId == dbId)?.Name;
        }

        public static async Task<Color?> GetDriveColor(DbId dbId)
        {
            var drives = _mockServerData.Drives;
            await SimulateNetworkDelay();
            return drives.FirstOrDefault(d => d.DbId == dbId)?.Color;
        }

        public static async Task<long?> GetDriveSize(DbId dbId)
        {
            var drives = _mockServerData.Drives;
            await SimulateNetworkDelay();
            return drives.FirstOrDefault(d => d.DbId == dbId)?.Size;
        }

        public static async Task<long?> GetDriveUsedSize(DbId dbId)
        {
            var drives = _mockServerData.Drives;
            await SimulateNetworkDelay();
            return drives.FirstOrDefault(d => d.DbId == dbId)?.UsedSize;
        }

        public static async Task<bool?> GetDriveIsActive(DbId dbId)
        {
            var drives = _mockServerData.Drives;
            await SimulateNetworkDelay();
            var drive = drives.FirstOrDefault(d => d.DbId == dbId);
            return drive != null ? drive.IsActive : (bool?)null;
        }

        public static async Task<bool?> GetDriveIsPaidOffer(DbId dbId)
        {
            var drives = _mockServerData.Drives;
            await SimulateNetworkDelay();
            var drive = drives.FirstOrDefault(d => d.DbId == dbId);
            return drive != null ? drive.IsPaidOffer : (bool?)null;
        }

        public static async Task<List<DbId>> GetDriveSyncsDbIds(DbId driveDbId)
        {
            var drives = _mockServerData.Drives;
            await SimulateNetworkDelay();
            return drives.FirstOrDefault(d => d.DbId == driveDbId)?.Syncs.Select(s => s!.DbId).ToList() ?? new List<DbId>();
        }

        // Sync-related requests
        public static async Task<SyncId?> GetSyncId(DbId dbId)
        {
            var syncs = _mockServerData.Syncs;
            await SimulateNetworkDelay();
            return syncs.FirstOrDefault(s => s.DbId == dbId)?.Id;
        }

        public static async Task<string?> GetSyncLocalPath(DbId dbId)
        {
            var syncs = _mockServerData.Syncs;
            await SimulateNetworkDelay();
            return syncs.FirstOrDefault(s => s.DbId == dbId)?.LocalPath;
        }

        public static async Task<string?> GetSyncRemotePath(DbId dbId)
        {
            var syncs = _mockServerData.Syncs;
            await SimulateNetworkDelay();
            return syncs.FirstOrDefault(s => s.DbId == dbId)?.RemotePath;
        }

        public static async Task<bool?> GetSyncSupportOfflineMode(DbId dbId)
        {
            var syncs = _mockServerData.Syncs;
            await SimulateNetworkDelay();
            return syncs.FirstOrDefault(s => s.DbId == dbId)?.SupportOnlineMode;
        }

        public static async Task<SyncType?> GetSyncType(DbId dbId)
        {
            var syncs = _mockServerData.Syncs;
            await SimulateNetworkDelay();
            return syncs.FirstOrDefault(s => s.DbId == dbId)?.SyncType;
        }

        public static async Task<List<User>> GetUsers() // USER_INFOLIST
        {
            App app = (App)Application.Current;
            //CommData data = await app.ComClient.SendRequest(((int)RequestNum.UserInfoList), new Dictionary<string, object>()).ConfigureAwait(false);
            /*ImmutableArray<byte> resultBytes = data.ResultByteArray;

            List<User> result = new List<User>();
            using (MemoryStream ms = new MemoryStream(resultBytes.ToArray()))
            using (BinaryReader reader = new BinaryReader(ms))
            {
                int exitCode = BinaryPrimitives.ReverseEndianness(reader.ReadInt32());
                if (exitCode != 0)
                {
                    Logger.Log(Logger.Level.Error, $"GetUsers: exitCode is {exitCode}.");
                    return result;
                }

                int count = 0;
                if (resultBytes.Length >= 4)
                {
                    count = reader.ReadInt32();

                }
                else
                {
                    Logger.Log(Logger.Level.Error, "GetUsers: resultBytes length is less than 4.");
                    return result;
                }

                for (int i = 0; i < count; i++)
                {
                    User user = await User.ReadFrom(reader);
                    result.Add(user);
                }
            }

            return result;*/
            return new List<User>();
        }

    }
}
