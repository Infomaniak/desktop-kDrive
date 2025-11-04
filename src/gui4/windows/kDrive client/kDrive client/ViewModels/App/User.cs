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
using DynamicData.Binding;
using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.Types;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Media.Imaging;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Documents;
using Windows.Storage.Streams;

namespace Infomaniak.kDrive.ViewModels
{
    public class User : UISafeObservableObject, IDisposable
    {
        private DbId _dbId = -1;
        private UserId _userId = -1;
        private string _name = "";
        private string _email = "";
        private byte[]? _avatar;
        private ImageSource? _avatarImageSource = null;
        private bool _isConnected = false;
        private bool _isStaff = false;
        private readonly ObservableCollection<Account> _accounts = new ObservableCollection<Account>();
        private ObservableCollection<DriveAvailable> _drivesAvailable = new ObservableCollection<DriveAvailable>();
        private readonly IDisposable _allDriveSubscribtion;
        public User(DbId dbId)
        {
            DbId = dbId;
            // Listen to changes in the accounts collection to update the drives collection
            _allDriveSubscribtion = _accounts.ToObservableChangeSet()
                     .TransformMany(a => a.Drives)
                     .Bind(out var allDrives)
                     .Subscribe();
            Drives = allDrives;
        }

        public void Dispose()
        {
            _allDriveSubscribtion.Dispose();
        }

        public DbId DbId
        {
            get => _dbId;
            set => SetPropertyInUIThread(ref _dbId, value);
        }

        public UserId UserId
        {
            get => _userId;
            set => SetPropertyInUIThread(ref _userId, value);
        }

        public string Name
        {
            get => _name;
            set => SetPropertyInUIThread(ref _name, value);
        }

        public string Email
        {
            get => _email;
            set => SetPropertyInUIThread(ref _email, value);
        }

        public byte[]? Avatar
        {
            get => _avatar;
            set
            {
                if (SetPropertyInUIThread(ref _avatar, value))
                {
                    AppModel.UIThreadDispatcher.TryEnqueue(async () =>
                    {
                        _avatarImageSource = await ByteArrayToImageSource(_avatar); // ByteArrayToImageSource need to be run in the UI thread
                        OnPropertyChanged(nameof(AvatarImageSource));
                    });
                }
            }
        }

        public ImageSource? AvatarImageSource
        {
            get => _avatarImageSource;
        }

        public bool IsConnected
        {
            get => _isConnected;
            set => SetPropertyInUIThread(ref _isConnected, value);
        }

        public bool IsStaff
        {
            get => _isStaff;
            set => SetPropertyInUIThread(ref _isStaff, value);
        }

        public ObservableCollection<Account> Accounts
        {
            get => _accounts;
        }

        public ObservableCollection<DriveAvailable> DrivesAvailable
        {
            get => _drivesAvailable;
        }

        public ReadOnlyObservableCollection<Drive> Drives
        {
            get;
        }

        // Combined collection of all drives (configured in db and available)
        public ObservableCollection<IDrive> AllDrives { get; } = new();



        public static async Task<ImageSource?> ByteArrayToImageSource(byte[]? imageData)
        {
            if (imageData == null || imageData.Length == 0)
                return null;

            using var stream = new InMemoryRandomAccessStream();
            await stream.WriteAsync(imageData.AsBuffer());
            stream.Seek(0);

            var bitmap = new BitmapImage();
            await bitmap.SetSourceAsync(stream);
            return bitmap;
        }

        public async Task RefreshAvailableDrives()
        {
            await App.ServiceProvider.GetRequiredService<IServerCommService>().RefreshUserDrivesAvailable(this.DbId, CancellationToken.None);
            MergeDrives();
        }
        private void MergeDrives()
        {
            // Define drive Equal action based on DriveId and type
            Func<IDrive, IDrive, bool> driveEqualAction = (d1, d2) =>
            {
                return d1.DriveId == d2.DriveId && d1.GetType() == d2.GetType();
            };


            List<IDrive> drives = new();
            foreach (var drive in Drives)
                drives.Add(drive);

            foreach (var drive in DrivesAvailable)
                if (drives.FirstOrDefault(d => d.DriveId == drive.DriveId) is null)
                    drives.Add(drive);

            for (int i = AllDrives.Count - 1; i >= 0; i--)
            {
                var drive = AllDrives[i];
                if (drives.FirstOrDefault(d => driveEqualAction(d, drive)) is null)
                    AllDrives.RemoveAt(i);
            }

            foreach (var drive in drives)
                if (AllDrives.FirstOrDefault(d => driveEqualAction(d, drive)) is null)
                    AllDrives.Add(drive);
        }
    }
}
