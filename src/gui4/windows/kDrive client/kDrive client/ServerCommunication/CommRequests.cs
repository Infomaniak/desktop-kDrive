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

using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
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

namespace Infomaniak.kDrive.ServerCommunication
{
    public class MockServerData
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
            Drives.Add(new Drive(1) { Id = 140946, Name = "Infomaniak", Color = Color.FromArgb(255, 0, 150, 136), Size = 1000000000, UsedSize = 250000000, IsActive = true, IsPaidOffer = false });
            Drives.Add(new Drive(2) { Id = 101, Name = "Test_kDrive", Color = Color.FromArgb(255, 156, 38, 176), Size = 2000000000, UsedSize = 150000000, IsActive = true, IsPaidOffer = false });
            Drives.Add(new Drive(3) { Id = 101, Name = "Test kDrive2", Color = Color.FromArgb(255, 255, 168, 44), Size = 2000000000, UsedSize = 150000000, IsActive = true, IsPaidOffer = false });
            Drives.Add(new Drive(4) { Id = 101, Name = "Test kDrive 3", Color = Color.FromArgb(255, 255, 168, 44), Size = 2000000000, UsedSize = 150000000, IsActive = true, IsPaidOffer = false });
            Drives.Add(new Drive(8) { Id = 101, Name = "Test kDrive 4", Color = Color.FromArgb(255, 156, 38, 176), Size = 2000000000, UsedSize = 150000000, IsActive = true, IsPaidOffer = false });
            Drives.Add(new Drive(9) { Id = 101, Name = "Test kDrive 5", Color = Color.FromArgb(255, 0, 150, 136), Size = 2000000000, UsedSize = 150000000, IsActive = true, IsPaidOffer = false });
            Users[0].Drives.Add(Drives[0]);
            //Users[0].Drives.Add(Drives[1]);
            //Users[0].Drives.Add(Drives[2]);
            //Users[0].Drives.Add(Drives[3]);
            Users[0].Drives.Add(Drives[4]);
            Users[0].Drives.Add(Drives[5]);


            Drives.Add(new Drive(5) { Id = 102, Name = "Etik_corp", Color = Color.Red, Size = 1000000000, UsedSize = 250000000, IsActive = true, IsPaidOffer = true });
            Users[1].Drives.Add(Drives[4]);

            // Create mock syncs
            Syncs.Add(new Sync(1, Drives[0]) { Id = 1000, LocalPath = "C:\\Users\\John\\kDrive", RemotePath = "", SupportOnlineMode = false });
            Drives[0].Syncs.Add(Syncs[0]);

            Syncs.Add(new Sync(2, Drives[1]) { Id = 1000, LocalPath = "C:\\Users\\John\\kDrive1", RemotePath = "", SupportOnlineMode = false });
            Drives[1].Syncs.Add(Syncs[1]);

            Syncs.Add(new Sync(2, Drives[2]) { Id = 1000, LocalPath = "C:\\Users\\John\\kDrive1", RemotePath = "", SupportOnlineMode = false });
            Drives[2].Syncs.Add(Syncs[1]);

            Syncs.Add(new Sync(2, Drives[3]) { Id = 1000, LocalPath = "C:\\Users\\John\\kDrive1", RemotePath = "", SupportOnlineMode = false });
            Drives[3].Syncs.Add(Syncs[1]);
        }
    }
    public class CommRequests
    {
        private enum RequestNum
        {
            LoginRequestToken = 1,
            UserDbIdList,
            UserInfoList,
            UserDelete,
            UserAvailableDrives,
            UserIdFromUserDbId,
            AccountInfoList,
            DriveInfoList,
            DriveInfo,
            DriveIdFromDriveDbId,
            DriveIdFromSyncDbId,
            DriveDefaultColor,
            DriveUpdate,
            DriveDelete,
            DriveGetOfflineFilesTotalSize,
            DriveSearch,
            SyncInfoList,
            SyncStart,
            SyncStop,
            SyncStatus,
            SyncIsRunning,
            SyncAdd,
            SyncAdd2,
            SyncStartAfterLogin,
            SyncDelete,
            SyncGetPublicLinkUrl,
            SyncGetPrivateLinkUrl,
            SyncAskForStatus,
            SyncSetSupportsVirtualFiles,
            SyncSetRootPinState,
            SyncPropagateSyncListChange,
            SyncNodeList,
            SyncNodeSetList,
            NodePath,
            NodeInfo,
            NodeSubfolders,
            NodeSubfolders2,
            NodeFolderSize,
            NodeCreateMissingFolders,
            ErrorInfoList,
            ErrorGetConflicts,
            ErrorDeleteServer,
            ErrorDeleteSync,
            ErrorDeleteInvalidToken,
            ErrorResolveConflicts,
            ErrorResolveUnsupportedChar,
            ExclTEMPLGetExcluded,
            ExclTEMPLGetList,
            ExclTEMPLSetList,
            ExclTEMPLPropagateChange,
            ParametersInfo,
            ParametersUpdate,
            UtilityFindGoodPathForNewSync,
            UtilityBestVFSAvailableMode,
            UtilityShowShortcut,
            UtilitySetShowShortcut,
            UtilityActivateLoadInfo,
            UtilityCheckCommStatus,
            UtilityHasSystemLaunchOnStartup,
            UtilityHasLaunchOnStartup,
            UtilitySetLaunchOnStartup,
            UtilitySetAppState,
            UtilityGetAppState,
            UtilitySendLogToSupport,
            UtilityCancelLogToSupport,
            UtilityGetLogEstimatedSize,
            UtilityCrash,
            UtilityQuit,
            UtilityDisplayClientReport, // Sent by the Client process as soon the UI is visible for the user.
            UpdaterChangeChannel,
            UpdaterVersionInfo,
            UpdaterState,
            UpdaterStartInstaller,
            UpdaterSkipVersion,
        }

        static private MockServerData _mockServerData = new MockServerData();

        private static async Task SimulateNetworkDelay()
        {
            await Task.Delay(50); // Simulate network delay
        }

        // User-related requests
        public static async Task<List<DbId>> GetUserDbIds()
        {
            var users = _mockServerData.Users;
            await SimulateNetworkDelay().ConfigureAwait(false);
            return users.Select(u => u.DbId).ToList();
        }

        public static async Task<UserId?> GetUserId(DbId dbId)
        {
            var users = _mockServerData.Users;
            await SimulateNetworkDelay().ConfigureAwait(false);
            return users.FirstOrDefault(u => u.DbId == dbId)?.Id;
        }

        public static async Task<string?> GetUserName(DbId dbId)
        {
            var users = _mockServerData.Users;
            await SimulateNetworkDelay().ConfigureAwait(false);
            return users.FirstOrDefault(u => u.DbId == dbId)?.Name;
        }

        public static async Task<string?> GetUserEmail(DbId dbId)
        {
            var users = _mockServerData.Users;
            await SimulateNetworkDelay().ConfigureAwait(false);
            return users.FirstOrDefault(u => u.DbId == dbId)?.Email;
        }

        public static async Task<Image?> GetUserAvatar(DbId dbId)
        {
            var users = await GetUsers().ConfigureAwait(false);
            return users.FirstOrDefault(u => u.DbId == dbId)?.Avatar;
        }

        public static async Task<bool?> GetUserIsConnected(DbId dbId)
        {
            var users = _mockServerData.Users;
            await SimulateNetworkDelay().ConfigureAwait(false);
            return users.FirstOrDefault(u => u.DbId == dbId)?.IsConnected;
        }

        public static async Task<bool?> GetUserIsStaff(DbId dbId)
        {
            var users = _mockServerData.Users;
            await SimulateNetworkDelay().ConfigureAwait(false);
            return users.FirstOrDefault(u => u.DbId == dbId)?.IsStaff;
        }

        public static async Task<List<DbId>> GetUserDrivesDbIds(DbId dbId)
        {
            var users = _mockServerData.Users;
            await SimulateNetworkDelay().ConfigureAwait(false);
            return users.FirstOrDefault(u => u.DbId == dbId)?.Drives.Select(d => d!.DbId).ToList() ?? new List<DbId>();
        }

        public static async Task<DbId> AddOrRelogUser(string token)
        {
            var users = _mockServerData.Users;
            await SimulateNetworkDelay().ConfigureAwait(false);
            return users.FirstOrDefault()?.DbId ?? -1;
        }

        // Drive-related requests
        public static async Task<DriveId?> GetDriveId(DbId dbId)
        {
            var drives = _mockServerData.Drives;
            await SimulateNetworkDelay().ConfigureAwait(false);
            return drives.FirstOrDefault(d => d.DbId == dbId)?.Id;
        }

        public static async Task<string?> GetDriveName(DbId dbId)
        {
            var drives = _mockServerData.Drives;
            await SimulateNetworkDelay().ConfigureAwait(false);
            return drives.FirstOrDefault(d => d.DbId == dbId)?.Name;
        }

        public static async Task<Color?> GetDriveColor(DbId dbId)
        {
            var drives = _mockServerData.Drives;
            await SimulateNetworkDelay().ConfigureAwait(false);
            return drives.FirstOrDefault(d => d.DbId == dbId)?.Color;
        }

        public static async Task<long?> GetDriveSize(DbId dbId)
        {
            var drives = _mockServerData.Drives;
            await SimulateNetworkDelay().ConfigureAwait(false);
            return drives.FirstOrDefault(d => d.DbId == dbId)?.Size;
        }

        public static async Task<long?> GetDriveUsedSize(DbId dbId)
        {
            var drives = _mockServerData.Drives;
            await SimulateNetworkDelay().ConfigureAwait(false);
            return drives.FirstOrDefault(d => d.DbId == dbId)?.UsedSize;
        }

        public static async Task<bool?> GetDriveIsActive(DbId dbId)
        {
            var drives = _mockServerData.Drives;
            await SimulateNetworkDelay().ConfigureAwait(false);
            var drive = drives.FirstOrDefault(d => d.DbId == dbId);
            return drive != null ? drive.IsActive : (bool?)null;
        }

        public static async Task<bool?> GetDriveIsPaidOffer(DbId dbId)
        {
            var drives = _mockServerData.Drives;
            await SimulateNetworkDelay().ConfigureAwait(false);
            var drive = drives.FirstOrDefault(d => d.DbId == dbId);
            return drive != null ? drive.IsPaidOffer : (bool?)null;
        }

        public static async Task<List<DbId>> GetDriveSyncsDbIds(DbId driveDbId)
        {
            var drives = _mockServerData.Drives;
            await SimulateNetworkDelay().ConfigureAwait(false);
            return drives.FirstOrDefault(d => d.DbId == driveDbId)?.Syncs.Select(s => s!.DbId).ToList() ?? new List<DbId>();
        }

        // Sync-related requests
        public static async Task<SyncId?> GetSyncId(DbId dbId)
        {
            var syncs = _mockServerData.Syncs;
            await SimulateNetworkDelay().ConfigureAwait(false);
            return syncs.FirstOrDefault(s => s.DbId == dbId)?.Id;
        }

        public static async Task<string?> GetSyncLocalPath(DbId dbId)
        {
            var syncs = _mockServerData.Syncs;
            await SimulateNetworkDelay().ConfigureAwait(false);
            return syncs.FirstOrDefault(s => s.DbId == dbId)?.LocalPath;
        }

        public static async Task<string?> GetSyncRemotePath(DbId dbId)
        {
            var syncs = _mockServerData.Syncs;
            await SimulateNetworkDelay().ConfigureAwait(false);
            return syncs.FirstOrDefault(s => s.DbId == dbId)?.RemotePath;
        }

        public static async Task<bool?> GetSyncSupportOfflineMode(DbId dbId)
        {
            var syncs = _mockServerData.Syncs;
            await SimulateNetworkDelay().ConfigureAwait(false);
            return syncs.FirstOrDefault(s => s.DbId == dbId)?.SupportOnlineMode;
        }

        public static async Task<SyncType?> GetSyncType(DbId dbId)
        {
            var syncs = _mockServerData.Syncs;
            await SimulateNetworkDelay().ConfigureAwait(false);
            return syncs.FirstOrDefault(s => s.DbId == dbId)?.SyncType;
        }

        public static async Task<List<User>> GetUsers() // USER_INFOLIST
        {
            App app = (App)Application.Current;
            CommData data = await app.ComClient.SendRequest(((int)RequestNum.UserInfoList), System.Collections.Immutable.ImmutableArray<byte>.Empty).ConfigureAwait(false);
            ImmutableArray<byte> resultBytes = data.ResultByteArray;

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
                    User user = User.ReadFrom(reader);
                    result.Add(user);
                }
            }

            return result;
        }

    }
}
