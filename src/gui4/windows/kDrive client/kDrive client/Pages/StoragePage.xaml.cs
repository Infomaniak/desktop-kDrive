/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

using Infomaniak.kDrive.Analytics;
using Infomaniak.kDrive.Converters;
using Infomaniak.kDrive.Pages.Settings;
using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.Pages
{
    public sealed partial class StoragePage : Page
    {
        private readonly IAnalyticsService _analyticsService = App.ServiceProvider.GetRequiredService<IAnalyticsService>();
        private readonly StoragePageViewModel _pageViewModel = new StoragePageViewModel();
        public StoragePageViewModel PageViewModel => _pageViewModel;
        public StoragePage()
        {
            Logger.LogInfo("Navigated to StoragePage - Initializing StoragePage components");
            InitializeComponent();
            SetupKeyboardAccelerators();
            Logger.LogDebug("StoragePage components initialized");
        }

        private void SetupKeyboardAccelerators()
        {
            KeyboardAcceleratorPlacementMode = Microsoft.UI.Xaml.Input.KeyboardAcceleratorPlacementMode.Hidden;
            AddRefreshAccelerator(Windows.System.VirtualKey.R);
            AddRefreshAccelerator(Windows.System.VirtualKey.F5);
        }

        private void AddRefreshAccelerator(Windows.System.VirtualKey key)
        {
            var accelerator = new Microsoft.UI.Xaml.Input.KeyboardAccelerator
            {
                Key = key,
                Modifiers = Windows.System.VirtualKeyModifiers.Control,

            };
            accelerator.Invoked += async (_, args) =>
            {
                args.Handled = true;
                try
                {
                    await PageViewModel.ForceUpdate(forceRefresh: true);
                }
                catch (OperationCanceledException)
                {
                    Logger.LogInfo("Disk size update was canceled.");
                }
            };
            KeyboardAccelerators.Add(accelerator);
        }
        protected override async void OnNavigatedTo(NavigationEventArgs e)
        {
            if (App.ServiceProvider.GetRequiredService<AppModel>().SelectedSync is null)
            {
                AppModel.UIThreadDispatcher.TryEnqueue(() => Frame.Navigate(typeof(SettingsPage)));
                return;
            }

            try
            {
                await PageViewModel.ForceUpdate(forceRefresh: true);
            }
            catch (OperationCanceledException)
            {
                Logger.LogInfo("Disk size update was canceled.");
            }

            _analyticsService.TrackPageView(Analytics.Keys.Category.StoragePage);
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
                    await PageViewModel.ForceUpdate(forceRefresh: true);
                    if (!PageViewModel.IsDiskConnected)
                    {
                        await Task.Delay(2000); // Add a small delay to let the user know we've done something but the disk still unavailable.
                    }
                }
                catch (OperationCanceledException)
                {
                    Logger.LogInfo("Disk size update was canceled.");
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
                Logger.LogWarning($"Failed to convert disk size {diskSize} to human readable string",
                    "StoragePage: Failed to format disk size");
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
                Logger.LogWarning($"Failed to convert usedSize size {usedSize} to human readable string",
                    "StoragePage: Failed to format used size");
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
                Logger.LogWarning($"Failed to convert usedSize size {usedSize} to human readable string",
                    "StoragePage: Failed to format used size");
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
                Logger.LogWarning($"Failed to parse number part '{numberPart}' from prettySize '{prettySize}'",
                    "StoragePage: Failed to parse formatted size");
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
        private long? _hydratedFilesSize;
        private long? _otherSyncsHydratedFilesSize;
        private long? _otherFileSize;
        private bool _loading = false;
        private string _diskRoot = "";
        private bool _isDiskConnected = false;
        private bool _isDisposed = false;
        private long? _onlineUsedSize;
        private long? _onlineFreeSize;
        private Sync? _sync = null;

        // Text
        private string _missingDiskTitle = "";
        private string _missingDiskSubtitle = "";

        private CancellationTokenSource _cancellationTokenSource = new CancellationTokenSource();

        public StoragePageViewModel()
        {
            AppViewModel.SelectedSyncChanged += OnSelectedSyncChanged;
            Sync = AppViewModel.SelectedSync;
            if (Sync is not null)
                Sync.Drive.PropertyChanged += OnDrivePropertyChanged;
        }

        private async void OnDrivePropertyChanged(object? sender, PropertyChangedEventArgs e)
        {
            if (e.PropertyName != nameof(Drive.UsedSize) && e.PropertyName != nameof(Drive.Size)) return;
            if (_isDisposed) return;
            await UpdateOnlineStorage(_cancellationTokenSource.Token);
        }

        private async void OnSelectedSyncChanged(object? sender, AppModel.SelectedSyncChangedEventArgs e)
        {
            if (Sync is not null)
                Sync.Drive.PropertyChanged -= OnDrivePropertyChanged;

            Sync = AppViewModel.SelectedSync;

            if (Sync is not null)
                Sync.Drive.PropertyChanged += OnDrivePropertyChanged;

            ResetDiskValues();
            await ForceUpdate(true);

        }

        private void ResetDiskValues()
        {
            DiskSize = null;
            DiskFreeSize = null;
            DiskUsedSize = null;
            HydratedFilesSize = null;
            OtherSyncsHydratedFilesSize = null;
            OtherFileSize = null;
            HasOtherSyncsOnSameDisk = false;
        }

        // IDisposable implementation
        public void Dispose()
        {
            if (!_isDisposed)
            {
                _isDisposed = true;

                AppViewModel.SelectedSyncChanged -= OnSelectedSyncChanged;

                if (Sync is not null)
                    Sync.Drive.PropertyChanged -= OnDrivePropertyChanged;

                _cancellationTokenSource.Cancel();
                _cancellationTokenSource.Dispose();
            }
        }

        bool _onlineUsedSizeIsLoading = false;
        public bool OnlineUsedSizeIsLoading
        {
            get => _onlineUsedSizeIsLoading;
            set => SetPropertyInUIThread(ref _onlineUsedSizeIsLoading, value);
        }

        public long? OnlineUsedSize
        {
            get => _onlineUsedSize;
            set => SetPropertyInUIThread(ref _onlineUsedSize, value);
        }

        public long? OnlineFreeSize
        {
            get => _onlineFreeSize;
            set => SetPropertyInUIThread(ref _onlineFreeSize, value);
        }

        public Sync? Sync
        {
            get => _sync;
            set => SetPropertyInUIThread(ref _sync, value);
        }

        public long? DiskSize
        {
            get => _diskSize;
            set => SetPropertyInUIThread(ref _diskSize, value);
        }

        public long? DiskUsedSize
        {
            get => _diskUsedSize;
            set
            {
                if (SetPropertyInUIThread(ref _diskUsedSize, value))
                    UpdateOtherFileSize();
            }
        }

        public long? DiskFreeSize
        {
            get => _diskFreeSize;
            set => SetPropertyInUIThread(ref _diskFreeSize, value);
        }

        public long? HydratedFilesSize
        {
            get => _hydratedFilesSize;
            set
            {
                if (SetPropertyInUIThread(ref _hydratedFilesSize, value))
                    UpdateOtherFileSize();
            }
        }
        public long? OtherSyncsHydratedFilesSize
        {
            get => _otherSyncsHydratedFilesSize;
            set
            {
                if (SetPropertyInUIThread(ref _otherSyncsHydratedFilesSize, value))
                    UpdateOtherFileSize();
            }
        }

        public long? OtherFileSize
        {
            get => _otherFileSize;
            set => SetPropertyInUIThread(ref _otherFileSize, value);
        }

        private bool _isHydratedFilesSizeLoading = false;
        public bool IsHydratedFilesSizeLoading
        {
            get => _isHydratedFilesSizeLoading;
            set
            {
                if (SetPropertyInUIThread(ref _isHydratedFilesSizeLoading, value))
                    OnPropertyChangedInUIThread(nameof(IsOtherFileSizeLoading));
            }
        }

        private bool _isOtherSyncsHydratedFilesSizeLoading = false;
        public bool IsOtherSyncsHydratedFilesSizeLoading
        {
            get => _isOtherSyncsHydratedFilesSizeLoading;
            set
            {
                if (SetPropertyInUIThread(ref _isOtherSyncsHydratedFilesSizeLoading, value))
                    OnPropertyChangedInUIThread(nameof(IsOtherFileSizeLoading));
            }
        }

        public bool IsOtherFileSizeLoading => IsHydratedFilesSizeLoading || IsOtherSyncsHydratedFilesSizeLoading;

        private bool _hasOtherSyncsOnSameDisk = false;
        public bool HasOtherSyncsOnSameDisk
        {
            get => _hasOtherSyncsOnSameDisk;
            set => SetPropertyInUIThread(ref _hasOtherSyncsOnSameDisk, value);
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

        public async Task ForceUpdate(bool forceRefresh)
        {

            if (_isDisposed)
                return;

            var currentSource = new CancellationTokenSource();
            var cancellationToken = currentSource.Token;
            var previousSource = Interlocked.Exchange(ref _cancellationTokenSource, currentSource);
            await previousSource.CancelAsync();
            previousSource.Dispose();
            if (_isDisposed || cancellationToken.IsCancellationRequested)
                return;

            Task onlineUpdate = UpdateOnlineStorage(cancellationToken);
            Task localUpdate = UpdateDiskSizeAsync(forceRefresh, cancellationToken);
            await Task.WhenAll(onlineUpdate, localUpdate);
        }

        public async Task UpdateOnlineStorage(CancellationToken cancellationToken)
        {
            if (_sync is null)
            {
                OnlineUsedSize = null;
                OnlineFreeSize = null;
                return;
            }

            OnlineUsedSizeIsLoading = true;

            // prefill with known values
            OnlineUsedSize = _sync.Drive.UsedSize;
            OnlineFreeSize = Math.Max(0, _sync.Drive.Size - _sync.Drive.UsedSize);

            try
            {
                await App.ServiceProvider.GetRequiredService<IServerCommService>().ActivateLoadInfo(cancellationToken); // Force the server to refresh
            }
            catch (OperationCanceledException)
            {
                Logger.LogInfo("ActivateLoadInfo cancelled");
            }

            // A newer update owns the state now.
            if (cancellationToken.IsCancellationRequested)
                return;

            OnlineUsedSize = _sync.Drive.UsedSize;
            OnlineFreeSize = Math.Max(0, _sync.Drive.Size - _sync.Drive.UsedSize);
            OnlineUsedSizeIsLoading = false;
        }

        public async Task UpdateDiskSizeAsync(bool forceRefresh, CancellationToken cancellationToken)
        {
            if (AppViewModel.SelectedSync == null)
                return;
            var sync = AppViewModel.SelectedSync;

            Loading = true;

            // Reset failed values (-1) to loading state (null); keep previously loaded values displayed while refreshing.
            HydratedFilesSize = (HydratedFilesSize == -1) ? null : HydratedFilesSize;
            OtherSyncsHydratedFilesSize = (OtherSyncsHydratedFilesSize == -1) ? null : OtherSyncsHydratedFilesSize;
            OtherFileSize = (OtherFileSize == -1) ? null : OtherFileSize;

            IsDiskConnected = false;
            var syncPath = sync.LocalPath;
            DiskRoot = Path.GetPathRoot(syncPath) ?? "";
            if (DiskRoot.Length == 0)
            {
                Logger.LogWarning($"Unable to get disk root of {syncPath}",
                    "StoragePageViewModel: Failed to get disk root");
                Utility.ShowUnexpectedErrorTeachingTip();
                DiskSize = -1;
                DiskFreeSize = -1;
                DiskUsedSize = -1;
                HydratedFilesSize = -1;
                OtherSyncsHydratedFilesSize = -1;
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

                var otherSyncs = SyncsOnSameDisk().Where(s => s != sync).ToList();
                HasOtherSyncsOnSameDisk = otherSyncs.Count > 0;
                if (otherSyncs.Count == 0)
                    OtherSyncsHydratedFilesSize = 0;

                IsHydratedFilesSizeLoading = true;
                IsOtherSyncsHydratedFilesSizeLoading = otherSyncs.Count > 0;

                Task currentSyncTask = LoadCurrentSyncHydratedFilesSizeAsync(sync, forceRefresh, cancellationToken);
                Task otherSyncsTask = LoadOtherSyncsHydratedFilesSizeAsync(otherSyncs, forceRefresh, cancellationToken);

                await Task.WhenAll(
                    currentSyncTask,
                    otherSyncsTask,
                    Task.Delay(forceRefresh ? 1000 : 0, CancellationToken.None)
                );
            }
            catch (System.IO.IOException ex)
            {
                Logger.LogInfo($"Error accessing drive info for root {DiskRoot}: {ex.Message}");
                IsDiskConnected = false;
            }
            catch (System.ArgumentException ex)
            {
                Logger.LogInfo($"Error accessing drive info for root {DiskRoot}: {ex.Message}");
                IsDiskConnected = false;
            }

            // A newer update owns the state now.
            if (cancellationToken.IsCancellationRequested)
                return;

            IsHydratedFilesSizeLoading = false;
            IsOtherSyncsHydratedFilesSizeLoading = false;
            Loading = false;
        }

        private async Task LoadCurrentSyncHydratedFilesSizeAsync(Sync sync, bool forceRefresh, CancellationToken cancellationToken)
        {
            HydratedFilesSize = sync.CachedHydratedFileSize;
            var result = await sync.GetHydratedFileSizeAsync(forceRefresh, cancellationToken);
            if (cancellationToken.IsCancellationRequested || result.ExitCode == ExitCode.OperationCanceled)
                return;

            if (result.ExitCode != ExitCode.Ok)
            {
                HydratedFilesSize = -1;
                Utility.ShowUnexpectedErrorTeachingTip();
                Logger.LogWarning($"Unable to get hydrated file size of {sync.LocalPath}",
                    "StoragePageViewModel: Failed to get hydrated file size");
                return;
            }

            HydratedFilesSize = result.Value;
            IsHydratedFilesSizeLoading = false;
        }

        private async Task LoadOtherSyncsHydratedFilesSizeAsync(List<Sync> otherSyncs, bool forceRefresh, CancellationToken cancellationToken)
        {
            if (otherSyncs.Count == 0)
                return;

            using var otherSyncsCts = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
            long total = 0;
            bool failed = false;
            // When refreshing an already loaded value, only publish the final total to avoid showing partial sums.
            bool progressive = OtherSyncsHydratedFilesSize is null;

            async Task LoadOneAsync(Sync otherSync)
            {
                var cachedSize = otherSync.CachedHydratedFileSize;
                total += cachedSize ?? 0;

                var result = await otherSync.GetHydratedFileSizeAsync(forceRefresh, otherSyncsCts.Token);
                if (failed || cancellationToken.IsCancellationRequested || result.ExitCode == ExitCode.OperationCanceled)
                    return;

                if (result.ExitCode != ExitCode.Ok)
                {
                    failed = true;
                    Logger.LogWarning($"Unable to get hydrated file size of {otherSync.LocalPath}",
                        "StoragePageViewModel: Failed to get other sync hydrated file size");
                    await otherSyncsCts.CancelAsync();
                    OtherSyncsHydratedFilesSize = -1;
                    IsOtherSyncsHydratedFilesSizeLoading = false;
                    return;
                }

                total -= cachedSize ?? 0;
                total += result.Value;
                if (progressive)
                    OtherSyncsHydratedFilesSize = total;
            }

            await Task.WhenAll(otherSyncs.Select(LoadOneAsync));
            if (failed || cancellationToken.IsCancellationRequested)
                return;

            OtherSyncsHydratedFilesSize = total;
            IsOtherSyncsHydratedFilesSizeLoading = false;
        }

        private List<Sync> SyncsOnSameDisk()
        {
            if (Sync is null)
                return new List<Sync>();
            string syncRoot = Path.GetPathRoot(Sync.LocalPath) ?? "";
            return AppViewModel.AllSyncs.Where(s => string.Equals(Path.GetPathRoot(s.LocalPath), syncRoot, StringComparison.OrdinalIgnoreCase)).ToList();
        }

        private void UpdateOtherFileSize()
        {
            if (!DiskUsedSize.HasValue || !HydratedFilesSize.HasValue || !OtherSyncsHydratedFilesSize.HasValue)
            {
                OtherFileSize = null;
                return;
            }

            OtherFileSize = DiskUsedSize - (Math.Max(0, HydratedFilesSize ?? 0) + Math.Max(0, OtherSyncsHydratedFilesSize ?? 0));
        }
    }
}
