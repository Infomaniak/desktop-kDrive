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

using kDrive_client.DataModel;
using Microsoft.UI.Xaml;
using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.Collections.Immutable;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace kDrive_client.ServerCommunication
{
    internal class MockServerData
    {
        public List<User> Users = new List<User>();
        public List<Drive> Drives = new List<Drive>();
        public List<Sync> Syncs = new List<Sync>();

        public MockServerData()
        {
            InitializeMockData();
        }

        public void InitializeMockData()
        {
            // Create mock users
            Users.Add(new User(1) { Id = 10, Name = "John", Email = "John.doe@infomaniak.com", IsConnected = true, IsStaff = false });
            Users.Add(new User(2) { Id = 11, Name = "Alice", Email = "Alice.doe@infomaniak.com", IsConnected = true, IsStaff = true });
            Users.Add(new User(3) { Id = 12, Name = "Bob", Email = "Bob.doe@infomaniak.com", IsConnected = false, IsStaff = false });

            // Create mock drives
            Drives.Add(new Drive(1) { Id = 100, Name = "Pro", Color = Color.Red, Size = 1000000000, UsedSize = 250000000 });
            Drives.Add(new Drive(2) { Id = 101, Name = "Photo", Color = Color.Blue, Size = 2000000000, UsedSize = 150000000 });
            Users[0].Drives.Add(Drives[0]);
            Users[0].Drives.Add(Drives[1]);

            Drives.Add(new Drive(3) { Id = 102, Name = "Pro1", Color = Color.Red, Size = 1000000000, UsedSize = 250000000 });
            Users[1].Drives.Add(Drives[2]);

            Drives.Add(new Drive(4) { Id = 103, Name = "Photo1", Color = Color.Blue, Size = 2000000000, UsedSize = 150000000 });
            Users[2].Drives.Add(Drives[3]);

            // Create mock syncs
            Syncs.Add(new Sync { DbId = 1, Id = 1000, LocalPath = "C:\\Users\\John\\kDrive\\Pro", RemotePath = "", SupportVfs = false });
            Syncs.Add(new Sync { DbId = 2, Id = 1001, LocalPath = "C:\\Users\\John\\kDrive\\Photo", RemotePath = "", SupportVfs = true });
            Drives[0].Syncs.Add(Syncs[0]);
            Drives[1].Syncs.Add(Syncs[1]);

            Syncs.Add(new Sync { DbId = 1, Id = 1000, LocalPath = "C:\\Users\\Alice\\kDrive\\Pro", RemotePath = "", SupportVfs = false });
            Drives[2].Syncs.Add(Syncs[2]);

            Syncs.Add(new Sync { DbId = 2, Id = 1001, LocalPath = "C:\\Users\\Bob\\kDrive\\Photo", RemotePath = "", SupportVfs = true });
            Drives[3].Syncs.Add(Syncs[3]);
        }
    }
    internal class CommRequests
    {
        private enum RequestNum
        {
            LOGIN_REQUESTTOKEN = 1,
            USER_DBIDLIST,
            USER_INFOLIST,
            USER_DELETE,
            USER_AVAILABLEDRIVES,
            USER_ID_FROM_USERDBID,
            ACCOUNT_INFOLIST,
            DRIVE_INFOLIST,
            DRIVE_INFO,
            DRIVE_ID_FROM_DRIVEDBID,
            DRIVE_ID_FROM_SYNCDBID,
            DRIVE_DEFAULTCOLOR,
            DRIVE_UPDATE,
            DRIVE_DELETE,
            DRIVE_GET_OFFLINE_FILES_TOTAL_SIZE,
            DRIVE_SEARCH,
            SYNC_INFOLIST,
            SYNC_START,
            SYNC_STOP,
            SYNC_STATUS,
            SYNC_ISRUNNING,
            SYNC_ADD,
            SYNC_ADD2,
            SYNC_START_AFTER_LOGIN,
            SYNC_DELETE,
            SYNC_GETPUBLICLINKURL,
            SYNC_GETPRIVATELINKURL,
            SYNC_ASKFORSTATUS,
            SYNC_SETSUPPORTSVIRTUALFILES,
            SYNC_SETROOTPINSTATE,
            SYNC_PROPAGATE_SYNCLIST_CHANGE,
            SYNCNODE_LIST,
            SYNCNODE_SETLIST,
            NODE_PATH,
            NODE_INFO,
            NODE_SUBFOLDERS,
            NODE_SUBFOLDERS2,
            NODE_FOLDER_SIZE,
            NODE_CREATEMISSINGFOLDERS,
            ERROR_INFOLIST,
            ERROR_GET_CONFLICTS,
            ERROR_DELETE_SERVER,
            ERROR_DELETE_SYNC,
            ERROR_DELETE_INVALIDTOKEN,
            ERROR_RESOLVE_CONFLICTS,
            ERROR_RESOLVE_UNSUPPORTED_CHAR,
            EXCLTEMPL_GETEXCLUDED,
            EXCLTEMPL_GETLIST,
            EXCLTEMPL_SETLIST,
            EXCLTEMPL_PROPAGATE_CHANGE,
            PARAMETERS_INFO,
            PARAMETERS_UPDATE,
            UTILITY_FINDGOODPATHFORNEWSYNC,
            UTILITY_BESTVFSAVAILABLEMODE,
            UTILITY_SHOWSHORTCUT,
            UTILITY_SETSHOWSHORTCUT,
            UTILITY_ACTIVATELOADINFO,
            UTILITY_CHECKCOMMSTATUS,
            UTILITY_HASSYSTEMLAUNCHONSTARTUP,
            UTILITY_HASLAUNCHONSTARTUP,
            UTILITY_SETLAUNCHONSTARTUP,
            UTILITY_SET_APPSTATE,
            UTILITY_GET_APPSTATE,
            UTILITY_SEND_LOG_TO_SUPPORT,
            UTILITY_CANCEL_LOG_TO_SUPPORT,
            UTILITY_GET_LOG_ESTIMATED_SIZE,
            UTILITY_CRASH,
            UTILITY_QUIT,
            UTILITY_DISPLAY_CLIENT_REPORT, // Sent by the Client process as soon the UI is visible for the user.
            UPDATER_CHANGE_CHANNEL,
            UPDATER_VERSION_INFO,
            UPDATER_STATE,
            UPDATER_START_INSTALLER,
            UPDATER_SKIP_VERSION,
        }

        static private MockServerData _mockServerData = new MockServerData();

        private static async Task SimulateNetworkDelay()
        {
            await Task.Delay(50); // Simulate network delay
        }

        // User-related requests
        public static async Task<List<int>> GetUserDbIds()
        {
            var users = _mockServerData.Users;
            await SimulateNetworkDelay().ConfigureAwait(false);
            return users.Select(u => u.DbId).ToList();
        }

        public static async Task<int?> GetUserId(int dbId)
        {
            var users = _mockServerData.Users;
            await SimulateNetworkDelay().ConfigureAwait(false);
            return users.FirstOrDefault(u => u.DbId == dbId)?.Id;
        }

        public static async Task<string?> GetUserName(int dbId)
        {
            var users = _mockServerData.Users;
            await SimulateNetworkDelay().ConfigureAwait(false);
            return users.FirstOrDefault(u => u.DbId == dbId)?.Name;
        }

        public static async Task<string?> GetUserEmail(int dbId)
        {
            var users = _mockServerData.Users;
            await SimulateNetworkDelay().ConfigureAwait(false);
            return users.FirstOrDefault(u => u.DbId == dbId)?.Email;
        }

        public static async Task<Image?> GetUserAvatar(int dbId)
        {
            var users = await GetUsers().ConfigureAwait(false);
            return users.FirstOrDefault(u => u.DbId == dbId)?.Avatar;
        }

        public static async Task<bool?> GetUserIsConnected(int dbId)
        {
            var users = _mockServerData.Users;
            await SimulateNetworkDelay().ConfigureAwait(false);
            return users.FirstOrDefault(u => u.DbId == dbId)?.IsConnected;
        }

        public static async Task<bool?> GetUserIsStaff(int dbId)
        {
            var users = _mockServerData.Users;
            await SimulateNetworkDelay().ConfigureAwait(false);
            return users.FirstOrDefault(u => u.DbId == dbId)?.IsStaff;
        }

        public static async Task<List<int>> GetUserDrivesDbIds(int dbId)
        {
            var users = _mockServerData.Users;
            await SimulateNetworkDelay().ConfigureAwait(false);
            return users.FirstOrDefault(u => u.DbId == dbId)?.Drives.Select(d => d!.DbId).ToList() ?? new List<int>();
        }

        // Drive-related requests
        public static async Task<int?> GetDriveId(int dbId)
        {
            var drives = _mockServerData.Drives;
            await SimulateNetworkDelay().ConfigureAwait(false);
            return drives.FirstOrDefault(d => d.DbId == dbId)?.Id;
        }

        public static async Task<string?> GetDriveName(int dbId)
        {
            var drives = _mockServerData.Drives;
            await SimulateNetworkDelay().ConfigureAwait(false);
            return drives.FirstOrDefault(d => d.DbId == dbId)?.Name;
        }

        public static async Task<Color?> GetDriveColor(int dbId)
        {
            var drives = _mockServerData.Drives;
            await SimulateNetworkDelay().ConfigureAwait(false);
            return drives.FirstOrDefault(d => d.DbId == dbId)?.Color;
        }

        public static async Task<long?> GetDriveSize(int dbId)
        {
            var drives = _mockServerData.Drives;
            await SimulateNetworkDelay().ConfigureAwait(false);
            return drives.FirstOrDefault(d => d.DbId == dbId)?.Size;
        }

        public static async Task<long?> GetDriveUsedSize(int dbId)
        {
            var drives = _mockServerData.Drives;
            await SimulateNetworkDelay().ConfigureAwait(false);
            return drives.FirstOrDefault(d => d.DbId == dbId)?.UsedSize;
        }

        public static async Task<bool?> GetDriveIsActive(int dbId)
        {
            var drives = _mockServerData.Drives;
            await SimulateNetworkDelay().ConfigureAwait(false);
            var drive = drives.FirstOrDefault(d => d.DbId == dbId);
            return drive != null ? drive.IsActive : (bool?)null;
        }

        public static async Task<List<User>> GetUsers() // USER_INFOLIST
        {
            App app = (App)Application.Current;
            CommData data = await app.ComClient.SendRequest(((int)RequestNum.USER_INFOLIST), System.Collections.Immutable.ImmutableArray<byte>.Empty).ConfigureAwait(false);
            ImmutableArray<byte> resultBytes = data.ResultByteArray;

            List<User> result = new List<User>();
            using (MemoryStream ms = new MemoryStream(resultBytes.ToArray()))
            using (Utility.BinaryReader reader = new Utility.BinaryReader(ms))
            {
                int exitCode = BinaryPrimitives.ReverseEndianness(reader.ReadInt32());
                if (exitCode != 0)
                {
                    Logger.LogError($"GetUsers: exitCode is {exitCode}.");
                    return result;
                }

                int count = 0;
                if (resultBytes.Length >= 4)
                {
                    count = reader.ReadInt32();

                }
                else
                {
                    Logger.LogError("GetUsers: resultBytes length is less than 4.");
                    return result;
                }

                for (int i = 0; i < count; i++)
                {
                    User user = User.ReadFrom(reader);
                    result.Add(user);
                }
            }

            return result;
        }
    }
}
