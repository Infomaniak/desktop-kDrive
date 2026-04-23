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

using Infomaniak.kDrive.Converters;
using Infomaniak.kDrive.Pages.Settings;
using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.Pages
{
    public sealed partial class StoragePage : Page
    {
        private readonly StoragePageViewModel _pageViewModel = new StoragePageViewModel();
        public StoragePageViewModel PageViewModel => _pageViewModel;
        public StoragePage()
        {
            Logger.Log(Logger.Level.Info, "Navigated to StoragePage - Initializing StoragePage components");
            InitializeComponent();
            Logger.Log(Logger.Level.Debug, "StoragePage components initialized");
        }
        protected override async void OnNavigatedTo(NavigationEventArgs e)
        {
            if (App.ServiceProvider.GetRequiredService<AppModel>().SelectedSync is null)
                AppModel.UIThreadDispatcher.TryEnqueue(() => Frame.Navigate(typeof(SettingsPage)));
            try
            {
                await PageViewModel.UpdateDiskSizeAsync().ConfigureAwait(false);
            }
            catch (OperationCanceledException)
            {
                Logger.Log(Logger.Level.Info, "Disk size update was canceled.");
            }
        }

        protected override void OnNavigatedFrom(NavigationEventArgs e)
        {
            PageViewModel.Dispose();
        }

        private async void RetryButton_click(object sender, RoutedEventArgs e)
        {
            if (sender is ButtonBase btn)
            {
                btn.IsEnabled = false;
                btn.Visibility = Visibility.Collapsed;
                RetryProgressRing.Visibility = Visibility.Visible;
                try
                {
                    await PageViewModel.UpdateDiskSizeAsync();
                    if (!PageViewModel.IsDiskConnected)
                    {
                        await Task.Delay(2000); // Add a small delay to let the user know we've done something but the disk still unavailable.
                    }
                }
                catch (OperationCanceledException)
                {
                    Logger.Log(Logger.Level.Info, "Disk size update was canceled.");
                }
                btn.IsEnabled = true;
                btn.Visibility = Visibility.Visible;
                RetryProgressRing.Visibility = Visibility.Collapsed;
            }
        }

        private string GetThisComputerTitle(string diskRoot, Int64? diskSize)
        {
            BytesToHumanReadableStringConverter converter = new BytesToHumanReadableStringConverter();

            string? prettySize = converter.Convert(diskSize ?? -1, typeof(string), "Decimals = 0", "") as string;
            if (prettySize is null)
            {
                Logger.Log(Logger.Level.Warning, $"Failed to convert disk size {diskSize} to human readable string");
                prettySize = $"{diskSize} bytes";
            }

            return Localizer.Instance.GetString("storageThisComputerTitle", diskRoot, prettySize);
        }

        private string GetStorageUsedLabel(Int64? usedSize)
        {
            BytesToHumanReadableStringConverter converter = new BytesToHumanReadableStringConverter();

            string? prettySize = converter.Convert(usedSize ?? -1, typeof(string), "Decimals = 0", "") as string;
            if (prettySize is null)
            {
                Logger.Log(Logger.Level.Warning, $"Failed to convert usedSize size {usedSize} to human readable string");
                prettySize = $"{usedSize} bytes";
            }
            return Localizer.Instance.GetStringWithPlural("labelStorageUsed", ParseNumberFromPrettySize(prettySize), prettySize);
        }

        private string GetStorageFreeLabel(Int64? usedSize)
        {
            BytesToHumanReadableStringConverter converter = new BytesToHumanReadableStringConverter();

            string? prettySize = converter.Convert(usedSize ?? -1, typeof(string), "Decimals = 0", "") as string;
            if (prettySize is null)
            {
                Logger.Log(Logger.Level.Warning, $"Failed to convert usedSize size {usedSize} to human readable string");
                prettySize = $"{usedSize} bytes";
            }

            return Localizer.Instance.GetStringWithPlural("labelStorageFree", ParseNumberFromPrettySize(prettySize), prettySize);
        }

        private int ParseNumberFromPrettySize(string prettySize)
        {
            // Extract the number part from the prettySize to use for pluralization
            var numberPart = new string(prettySize.TakeWhile(c => char.IsDigit(c) || c == '.' || c == ',').ToArray());
            numberPart = numberPart.Replace(',', '.'); // Ensure the decimal separator is a dot for parsing
            double number;
            if (!double.TryParse(numberPart, System.Globalization.NumberStyles.Any, System.Globalization.CultureInfo.InvariantCulture, out number))
            {
                Logger.Log(Logger.Level.Warning, $"Failed to parse number part '{numberPart}' from prettySize '{prettySize}'");
                number = 2; // Default to 2 to use plural form if parsing fails
            }
            return (int)Math.Round(number);
        }
    }



    public partial class StoragePageViewModel : UISafeObservableObject, IDisposable
    {
        private readonly AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        public AppModel AppViewModel => _viewModel;

        private long? _diskSize;
        private long? _diskUsedSize;
        private long? _diskFreeSize;
        private long? _hydratedFileSize;
        private long? _otherFileSize;
        private bool _loading = false;
        private string _diskRoot = "";
        private bool _isDiskConnected = false;
        private bool _isDisposed = false;

        // Text
        private string _missingDiskTitle = "";
        private string _missingDiskSubtitle = "";

        private CancellationTokenSource _cancellationTokenSource = new CancellationTokenSource();

        public StoragePageViewModel()
        {
            AppViewModel.SelectedSyncChanged += OnSelectedSyncChanged;
        }

        private async void OnSelectedSyncChanged(object? sender, AppModel.SelectedSyncChangedEventArgs e)
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

        // IDisposable implementation
        public void Dispose()
        {
            if (!_isDisposed)
            {
                _isDisposed = true;

                AppViewModel.SelectedSyncChanged -= OnSelectedSyncChanged;
                _cancellationTokenSource.Cancel();
                _cancellationTokenSource.Dispose();
            }
        }

        public long? DiskSize
        {
            get => _diskSize;
            set => SetPropertyInUIThread(ref _diskSize, value);
        }

        public long? DiskUsedSize
        {
            get => _diskUsedSize;
            set => SetPropertyInUIThread(ref _diskUsedSize, value);
        }

        public long? DiskFreeSize
        {
            get => _diskFreeSize;
            set => SetPropertyInUIThread(ref _diskFreeSize, value);
        }

        public long? HydratedFileSize
        {
            get => _hydratedFileSize;
            set => SetPropertyInUIThread(ref _hydratedFileSize, value);
        }

        public long? OtherFileSize
        {
            get => _otherFileSize;
            set => SetPropertyInUIThread(ref _otherFileSize, value);
        }

        public bool Loading
        {
            get => _loading;
            set => SetPropertyInUIThread(ref _loading, value);
        }

        public string DiskRoot
        {
            get => _diskRoot;
            set
            {
                SetPropertyInUIThread(ref _diskRoot, value);
                OnPropertyChangedInUIThread(nameof(TrimmedDiskRoot));
            }
        }

        public string TrimmedDiskRoot
        {
            get => DiskRoot.TrimEnd(Path.DirectorySeparatorChar);
        }

        public string MissingDiskTitle
        {
            get => _missingDiskTitle;
            set => SetPropertyInUIThread(ref _missingDiskTitle, value);
        }

        public string MissingDiskSubtitle
        {
            get => _missingDiskSubtitle;
            set => SetPropertyInUIThread(ref _missingDiskSubtitle, value);
        }

        public bool IsDiskConnected
        {
            get => _isDiskConnected;
            set => SetPropertyInUIThread(ref _isDiskConnected, value);
        }

        public async Task UpdateDiskSizeAsync()
        {
            if (AppViewModel.SelectedSync == null)
                return;
            var sync = AppViewModel.SelectedSync;
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
            if (DiskRoot.Length == 0)
            {
                Logger.Log(Logger.Level.Warning, $"Unable to get disk root of {syncPath}");
                Utility.ShowUnexpectedErrorTeachingTip();
                DiskSize = -1;
                DiskFreeSize = -1;
                DiskUsedSize = -1;
                HydratedFileSize = -1;
                OtherFileSize = -1;
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
                if (HydratedFileSize is null)
                {
                    Loading = false;
                    HydratedFileSize = -1;
                    OtherFileSize = -1;
                    if (!_cancellationTokenSource.IsCancellationRequested)
                    {
                        Utility.ShowUnexpectedErrorTeachingTip();
                        Logger.Log(Logger.Level.Warning, $"Unable to get hydrated file size of {syncPath}");
                    }
                    return;
                }
                OtherFileSize = DiskUsedSize - HydratedFileSize;
            }
            catch (System.IO.IOException ex)
            {
                Logger.Log(Logger.Level.Info, $"Error accessing drive info for root {DiskRoot}: {ex.Message}");
                IsDiskConnected = false;
            }
            Loading = false;
        }

        private async Task<long?> GetHydratedFileSizeAsync(CancellationToken cancellationToken)
        {
            if (AppViewModel.SelectedSync is null)
                return null;

            var commService = App.ServiceProvider.GetRequiredService<IServerCommService>();
            UInt64? res = await commService.GetSyncOfflineFilesSize(AppViewModel.SelectedSync.DbId, cancellationToken);

            if (res is null)
            {
                if (!cancellationToken.IsCancellationRequested)
                    Logger.Log(Logger.Level.Warning, "GetSyncOfflineFilesSize returned null");
                return null;
            }

            if (res.Value > long.MaxValue)
                return long.MaxValue;
            else
                return (long)res.Value;
        }
    }
}
