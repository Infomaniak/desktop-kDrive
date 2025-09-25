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

using CommunityToolkit.Mvvm.ComponentModel;
using Infomaniak.kDrive.Pages.Onboarding;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.IO;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.Pages
{
    public sealed partial class StoragePage : Page
    {
        private StoragePageViewModel _pageViewModel = new StoragePageViewModel();
        public StoragePageViewModel PageViewModel => _pageViewModel;
        public StoragePage()
        {
            Logger.Log(Logger.Level.Info, "Navigated to StoragePage - Initializing StoragePage components");
            InitializeComponent();
            Logger.Log(Logger.Level.Debug, "StoragePage components initialized");
        }

        protected async override void OnNavigatedTo(NavigationEventArgs e)
        {
            try
            {
                await PageViewModel.UpdateDiskSizeAsync().ConfigureAwait(false);
            }
            catch (OperationCanceledException)
            {
                Logger.Log(Logger.Level.Info, "Disk size update was canceled.");
            }
        }

        private async void RetryButton_click(object sender, RoutedEventArgs e)
        {
            if (sender is Button btn)
            {
                btn.IsEnabled = false;
                try
                {
                    await PageViewModel.UpdateDiskSizeAsync().ConfigureAwait(false);
                    if (!PageViewModel.IsDiskConnected)
                    {
                        await Task.Delay(2000); // Add a small delay to let the user now we've done something but the disk still unavailable.
                    }
                }
                catch (OperationCanceledException)
                {
                    Logger.Log(Logger.Level.Info, "Disk size update was canceled.");
                }
                btn.IsEnabled = true;
            }
        }
    }

    public class StoragePageViewModel : ObservableObject
    {
        private readonly AppModel _viewModel = ((App)Application.Current).Data;
        public AppModel AppViewModel => _viewModel;

        private double? _diskSize;
        private double? _diskUsedSize;
        private double? _diskFreeSize;
        private double? _hydratedFileSize;
        private double? _otherFileSize;
        private bool _loading = false;
        private string _diskRoot = "";
        private bool _isDiskConnected = false;

        private CancellationTokenSource _cancellationTokenSource = new CancellationTokenSource();

        public StoragePageViewModel()
        {
            AppViewModel.PropertyChanged += async (s, e) =>
            {
                if (e.PropertyName == nameof(AppViewModel.SelectedSync))
                {
                    try
                    {
                        await UpdateDiskSizeAsync();
                    }
                    catch (OperationCanceledException)
                    {
                        Logger.Log(Logger.Level.Info, "Disk size update was canceled.");
                    }
                }
            };
        }

        ~StoragePageViewModel()
        {
            _cancellationTokenSource.Cancel();
            _cancellationTokenSource.Dispose();
        }

        public double? DiskSize
        {
            get => _diskSize;
            set => SetProperty(ref _diskSize, value);
        }

        public double? DiskUsedSize
        {
            get => _diskUsedSize;
            set => SetProperty(ref _diskUsedSize, value);
        }

        public double? DiskFreeSize
        {
            get => _diskFreeSize;
            set => SetProperty(ref _diskFreeSize, value);
        }

        public double? HydratedFileSize
        {
            get => _hydratedFileSize;
            set => SetProperty(ref _hydratedFileSize, value);
        }

        public double? OtherFileSize
        {
            get => _otherFileSize;
            set => SetProperty(ref _otherFileSize, value);
        }

        public bool Loading
        {
            get => _loading;
            set => SetProperty(ref _loading, value);
        }

        public string DiskRoot
        {
            get => _diskRoot;
            set => SetProperty(ref _diskRoot, value);
        }

        public bool IsDiskConnected
        {
            get => _isDiskConnected;
            set => SetProperty(ref _isDiskConnected, value);
        }

        public async Task UpdateDiskSizeAsync()
        {
            if (AppViewModel.SelectedSync == null)
                return;
            var sync = AppViewModel.SelectedSync;
            if (sync == null)
                return;
            if (Loading)
            {
                await _cancellationTokenSource.CancelAsync();
                _cancellationTokenSource.Dispose();
                _cancellationTokenSource = new CancellationTokenSource();
            }
            Loading = true;
            DiskSize = null;
            DiskFreeSize = null;
            DiskUsedSize = null;
            HydratedFileSize = null;
            OtherFileSize = null;
            IsDiskConnected = false;
            var syncPath = sync.LocalPath;
            DiskRoot = Path.GetPathRoot(syncPath) ?? "";
            if (DiskRoot == "")
            {
                Logger.Log(Logger.Level.Warning, $"Unable to get disk root of {syncPath}");
                Loading = false;
                return;
            }
            try
            {
                DriveInfo driveInfo = new DriveInfo(DiskRoot);
                IsDiskConnected = true;
                DiskSize = driveInfo.TotalSize;
                DiskFreeSize = driveInfo.AvailableFreeSpace;
                DiskUsedSize = DiskSize - DiskFreeSize;
                HydratedFileSize = await GetHydratedFileSizeAsync(_cancellationTokenSource.Token);
                if (HydratedFileSize == null)
                {
                    Loading = false;
                    Logger.Log(Logger.Level.Warning, $"Unable to get hydrated file size of {syncPath}");
                    return;
                }
                OtherFileSize = DiskUsedSize - HydratedFileSize;
            }
            catch (System.IO.DriveNotFoundException ex)
            {
                Logger.Log(Logger.Level.Info, $"Error accessing drive info for root {DiskRoot}: {ex.Message}");
                IsDiskConnected = false;
            }
            Loading = false;
        }

        private async Task<double?> GetHydratedFileSizeAsync(CancellationToken cancellationToken)
        {
            await Task.Delay(5000, cancellationToken);
            return new Random().NextInt64((long?)DiskUsedSize ?? 0);
        }
    }
}
