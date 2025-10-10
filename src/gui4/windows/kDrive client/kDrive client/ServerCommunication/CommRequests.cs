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

using DynamicData;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Infomaniak.kDrive.ViewModels.Errors;
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

            // Create mock drives
            Drives.Add(new Drive(1, Users[0]) { Id = 140946, Name = "Infomaniak", Color = Color.FromArgb(255, 0, 150, 136), Size = 1000000000, UsedSize = 250000000, IsActive = true, IsPaidOffer = true });
            Drives.Add(new Drive(2, Users[0]) { Id = 101, Name = "Etik corp", Color = Color.FromArgb(255, 156, 38, 176), Size = 2000000000, UsedSize = 150000000, IsActive = true, IsPaidOffer = true });
            Drives.Add(new Drive(3, Users[0]) { Id = 102, Name = "CH corp", Color = Color.FromArgb(255, 110, 168, 44), Size = 2000000000, UsedSize = 150000000, IsActive = true, IsPaidOffer = false });
            Drives.Add(new Drive(4, Users[0]) { Id = 103, Name = "The cloud", Color = Color.FromArgb(255, 255, 168, 110), Size = 2000000000, UsedSize = 150000000, IsActive = true, IsPaidOffer = false });
            Drives.Add(new Drive(5, Users[0]) { Id = 104, Name = "SwissCloud", Color = Color.FromArgb(255, 160, 168, 213), Size = 2000000000, UsedSize = 150000000, IsActive = true, IsPaidOffer = false });
            Drives.Add(new Drive(6, Users[0]) { Id = 105, Name = "FrenchCloud", Color = Color.FromArgb(255, 123, 179, 12), Size = 2000000000, UsedSize = 150000000, IsActive = true, IsPaidOffer = false });
            Drives.Add(new Drive(7, Users[0]) { Id = 106, Name = "EuropaCloud", Color = Color.FromArgb(255, 160, 12, 213), Size = 2000000000, UsedSize = 150000000, IsActive = true, IsPaidOffer = false });
            Drives.Add(new Drive(8, Users[0]) { Id = 107, Name = "WinUI cloud", Color = Color.FromArgb(255, 12, 168, 179), Size = 2000000000, UsedSize = 150000000, IsActive = true, IsPaidOffer = false });

            Users[0].Drives.Add(Drives[0]);
            Users[0].Drives.Add(Drives[1]);
            Users[0].Drives.Add(Drives[2]);
            Users[0].Drives.Add(Drives[3]);
            Users[0].Drives.Add(Drives[4]);
            Users[0].Drives.Add(Drives[5]);
            Users[0].Drives.Add(Drives[6]);
            Users[0].Drives.Add(Drives[7]);


            // Create mock syncs
            Syncs.Add(new Sync(1, Drives[0]) { Id = 1000, LocalPath = "C:\\Users\\John\\Etik corp sync1", RemotePath = "", SupportOnlineMode = false });
            Drives[0].Syncs.Add(Syncs[0]);

            Syncs.Add(new Sync(2, Drives[2]) { Id = 1001, LocalPath = "D:\\Users\\John\\CH corp\\kDrive Metier", RemotePath = "", SupportOnlineMode = false });
            Drives[1].Syncs.Add(Syncs[1]);

            Syncs.Add(new Sync(3, Drives[2]) { Id = 1002, LocalPath = "F:\\Users\\John\\CH corp\\kDrive Adminstration", RemotePath = "", SupportOnlineMode = false });
            Syncs.Add(new Sync(4, Drives[2]) { Id = 1003, LocalPath = "F:\\Users\\John\\Music", RemotePath = "", SupportOnlineMode = false });
            Drives[2].Syncs.Add(Syncs[2]);
            Drives[2].Syncs.Add(Syncs[3]);

            Syncs.Add(new Sync(5, Drives[3]) { Id = 1004, LocalPath = "F:\\Users\\John\\Photos", RemotePath = "", SupportOnlineMode = false });
            Syncs.Add(new Sync(6, Drives[3]) { Id = 1005, LocalPath = "F:\\Users\\John\\Famille\\Photos", RemotePath = "", SupportOnlineMode = false });
            Syncs.Add(new Sync(7, Drives[3]) { Id = 1006, LocalPath = "F:\\Users\\John\\vidéo", RemotePath = "", SupportOnlineMode = false });
            Drives[3].Syncs.Add(Syncs[4]);
            Drives[3].Syncs.Add(Syncs[5]);
            Drives[3].Syncs.Add(Syncs[6]);


            Syncs.Add(new Sync(8, Drives[4]) { Id = 1007, LocalPath = "F:\\Users\\John\\Film", RemotePath = "", SupportOnlineMode = false });
            Syncs.Add(new Sync(9, Drives[5]) { Id = 1008, LocalPath = "F:\\Users\\John\\Pro\\Comptabilité", RemotePath = "", SupportOnlineMode = false });
            Syncs.Add(new Sync(10, Drives[6]) { Id = 1009, LocalPath = "F:\\Users\\John\\Pro\\Rh", RemotePath = "", SupportOnlineMode = false });
            Syncs.Add(new Sync(11, Drives[7]) { Id = 1010, LocalPath = "F:\\Users\\John\\The cloud sync8", RemotePath = "", SupportOnlineMode = false });
            Drives[4].Syncs.Add(Syncs[7]);
            Drives[5].Syncs.Add(Syncs[8]);
            Drives[6].Syncs.Add(Syncs[9]);
            Drives[7].Syncs.Add(Syncs[10]);


            Syncs.Add(new Sync(13, Drives[4]) { Id = 1013, LocalPath = "F:\\Users\\John\\SwissCloud 1", RemotePath = "", SupportOnlineMode = false });
            Syncs.Add(new Sync(14, Drives[4]) { Id = 1014, LocalPath = "F:\\Users\\John\\SwissCloud 2", RemotePath = "", SupportOnlineMode = false });
            Syncs.Add(new Sync(15, Drives[4]) { Id = 1015, LocalPath = "F:\\Users\\John\\SwissCloud 3", RemotePath = "", SupportOnlineMode = false });
            Syncs.Add(new Sync(16, Drives[4]) { Id = 1016, LocalPath = "F:\\Users\\John\\SwissCloud 4", RemotePath = "", SupportOnlineMode = false });
            Syncs.Add(new Sync(17, Drives[4]) { Id = 1017, LocalPath = "F:\\Users\\John\\SwissCloud 5", RemotePath = "", SupportOnlineMode = false });
            Syncs.Add(new Sync(18, Drives[4]) { Id = 1018, LocalPath = "F:\\Users\\John\\SwissCloud 6", RemotePath = "", SupportOnlineMode = false });
            Syncs.Add(new Sync(19, Drives[4]) { Id = 1018, LocalPath = "F:\\Users\\John\\SwissCloud 7", RemotePath = "", SupportOnlineMode = false });
            Drives[4].Syncs.Add(Syncs[11]);
            Drives[4].Syncs.Add(Syncs[12]);
            Drives[4].Syncs.Add(Syncs[13]);
            Drives[4].Syncs.Add(Syncs[14]);
            Drives[4].Syncs.Add(Syncs[15]);
            Drives[4].Syncs.Add(Syncs[16]);
            Drives[4].Syncs.Add(Syncs[17]);

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
            await Task.Delay(100); // Simulate network delay
        }

        // User-related requests
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
            return users.FirstOrDefault(u => u.DbId == dbId)?.Id;
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

        public static async Task<Image?> GetUserAvatar(DbId dbId)
        {
            //var users = await GetUsers().ConfigureAwait(false);
            var users = _mockServerData.Users;
            await SimulateNetworkDelay();
            return users.FirstOrDefault(u => u.DbId == dbId)?.Avatar;
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
            return users.FirstOrDefault(u => u.DbId == dbId)?.Drives.Select(d => d!.DbId).ToList() ?? new List<DbId>();
        }

        public static async Task<DbId> AddOrRelogUser(string token)
        {
            var users = _mockServerData.Users;
            await SimulateNetworkDelay();
            return users.FirstOrDefault()?.DbId ?? -1;
        }

        // Drive-related requests
        public static async Task<DriveId?> GetDriveId(DbId dbId)
        {
            var drives = _mockServerData.Drives;
            await SimulateNetworkDelay();
            return drives.FirstOrDefault(d => d.DbId == dbId)?.Id;
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
