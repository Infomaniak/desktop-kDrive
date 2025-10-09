using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.ServerCommunication.Services;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using System.Collections.Generic;
using System.Drawing;

namespace Infomaniak.kDrive.ServerCommunication.MockData
{
    public class MockServerData
    {
        public List<User> Users { get; } = new List<User>();
        public List<Account> Accounts { get; } = new List<Account>();
        public List<Drive> Drives { get; } = new List<Drive>();
        public List<Sync> Syncs { get; } = new List<Sync>();

        public MockServerData()
        {
            InitializeMockData();
        }

        private void InitializeMockData()
        {
            // Create mock users
            Users.Add(new User(1) { UserId = 10, Name = "John", Email = "John.doe@infomaniak.com", IsConnected = true, IsStaff = false });
            Users.Add(new User(2) { UserId = 11, Name = "Alice", Email = "Alice.doe@infomaniak.com", IsConnected = false, IsStaff = false });

            // Create mock accounts
            Accounts.Add(new Account(1, Users[0]));
            Accounts.Add(new Account(2, Users[1]));
            Users[0].Accounts.Add(Accounts[0]);
            Users[1].Accounts.Add(Accounts[1]);

            // Create mock drives
            Drives.Add(new Drive(1, Accounts[0]) { DriveId = 140946, Name = "Infomaniak", Color = Color.FromArgb(255, 0, 150, 136), Size = 1000000000, UsedSize = 250000000, IsActive = true, IsPaidOffer = true });
            Drives.Add(new Drive(2, Accounts[0]) { DriveId = 101, Name = "Etik corp", Color = Color.FromArgb(255, 156, 38, 176), Size = 2000000000, UsedSize = 150000000, IsActive = true, IsPaidOffer = true });
            Drives.Add(new Drive(3, Accounts[0]) { DriveId = 102, Name = "CH corp", Color = Color.FromArgb(255, 110, 168, 44), Size = 2000000000, UsedSize = 150000000, IsActive = true, IsPaidOffer = false });
            Drives.Add(new Drive(4, Accounts[0]) { DriveId = 103, Name = "The cloud", Color = Color.FromArgb(255, 255, 168, 110), Size = 2000000000, UsedSize = 150000000, IsActive = true, IsPaidOffer = false });
            Drives.Add(new Drive(5, Accounts[0]) { DriveId = 104, Name = "SwissCloud", Color = Color.FromArgb(255, 160, 168, 213), Size = 2000000000, UsedSize = 150000000, IsActive = true, IsPaidOffer = false });
            Drives.Add(new Drive(6, Accounts[0]) { DriveId = 105, Name = "FrenchCloud", Color = Color.FromArgb(255, 123, 179, 12), Size = 2000000000, UsedSize = 150000000, IsActive = true, IsPaidOffer = false });
            Drives.Add(new Drive(7, Accounts[1]) { DriveId = 106, Name = "EuropaCloud", Color = Color.FromArgb(255, 160, 12, 213), Size = 2000000000, UsedSize = 150000000, IsActive = true, IsPaidOffer = false });
            Drives.Add(new Drive(8, Accounts[1]) { DriveId = 107, Name = "WinUI cloud", Color = Color.FromArgb(255, 12, 168, 179), Size = 2000000000, UsedSize = 150000000, IsActive = true, IsPaidOffer = false });

            Accounts[0].Drives.Add(Drives[0]);
            Accounts[0].Drives.Add(Drives[1]);
            Accounts[0].Drives.Add(Drives[2]);
            Accounts[0].Drives.Add(Drives[3]);
            Accounts[0].Drives.Add(Drives[4]);
            Accounts[0].Drives.Add(Drives[5]);
            Accounts[1].Drives.Add(Drives[6]);
            Accounts[1].Drives.Add(Drives[7]);


            // Create mock syncs
            Syncs.Add(new Sync(1, Drives[0]) { Id = 1000, LocalPath = "C:\\Users\\John\\Etik corp sync1", RemotePath = "", SupportOnlineMode = false });
            Drives[0].Syncs.Add(Syncs[0]);

            Syncs.Add(new Sync(2, Drives[1]) { Id = 1001, LocalPath = "D:\\Users\\John\\CH corp\\kDrive Metier", RemotePath = "", SupportOnlineMode = false });
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
}
