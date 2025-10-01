using Infomaniak.kDrive.Pages.Onboarding;
using Infomaniak.kDrive.ViewModels;
using Infomaniak.kDrive.OnBoarding;
using Infomaniak.kDrive.Types;
using Microsoft.UI;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Data;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Threading.Tasks;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.UI;
using Windows.Storage.Pickers;

namespace Infomaniak.kDrive.Pages.Onboarding
{
    public sealed partial class DriveSelectionPage : Page
    {
        private AppModel _viewModel = ((App)Application.Current).Data;
        private ViewModels.Onboarding? _onBoardingViewModel;
        private Dictionary<Sync, string> _previousSyncPaths = new Dictionary<Sync, string>(); // To store previous sync paths and allow reverting if needed in advanced settings
        public AppModel ViewModel { get { return _viewModel; } }
        public ViewModels.Onboarding? ObViewModel { get => _onBoardingViewModel; }
        public DriveSelectionPage()
        {
            Logger.Log(Logger.Level.Info, "Navigated to WelcomePage - Initializing WelcomePage components");
            InitializeComponent();
            Logger.Log(Logger.Level.Debug, "WelcomePage components initialized");
        }

        protected override void OnNavigatedTo(NavigationEventArgs e)
        {
            if (e.Parameter is ViewModels.Onboarding obvm)
            {
                _onBoardingViewModel = obvm;
            }
            else
            {
                Logger.Log(Logger.Level.Fatal, "OnBoardingViewModel parameter missing when navigating to DriveSelectionPage");
                throw new Exception("OnBoardingViewModel parameter missing when navigating to DriveSelectionPage");
            }
        }

        private void DriveListCheckBox_Checked(object sender, RoutedEventArgs e)
        {
            if (sender is CheckBox cb && cb.DataContext is Drive drive && _onBoardingViewModel != null)
            {
                string localPath = Utility.DefaultSyncPath(drive.Name);
                Sync newSync = new Sync(-1, drive)
                {
                    LocalPath = localPath,
                    SupportOnlineMode = true,
                    SyncType = Utility.SupportOnlineSync(localPath) ? SyncType.Online : SyncType.Offline
                };
                _onBoardingViewModel.NewSyncs.Add(newSync);
            }
        }

        private void DriveListCheckBox_Unchecked(object sender, RoutedEventArgs e)
        {
            if (sender is CheckBox cb && cb.DataContext is Drive drive && _onBoardingViewModel != null)
            {
                var syncToRemove = _onBoardingViewModel.NewSyncs.FirstOrDefault(s => s.Drive == drive);
                if (syncToRemove != null)
                {
                    _onBoardingViewModel.NewSyncs.Remove(syncToRemove);
                }
                else
                {
                    Logger.Log(Logger.Level.Warning, "Drive to remove not found in NewSyncs list");
                }
            }
        }

        private void AdvancedSettingsDialog_CloseButtonClick(ContentDialog sender, ContentDialogButtonClickEventArgs args)
        {
            Logger.Log(Logger.Level.Info, "Advanced settings dialog closed, reverting any unsaved changes.");
            if (_onBoardingViewModel == null)
            {
                Logger.Log(Logger.Level.Error, "OnBoardingViewModel is null in AdvancedSettingsDialog_CloseButtonClick");
                return;
            }

            // Revert any changes
            foreach (var sync in _onBoardingViewModel.NewSyncs)
            {
                if (_previousSyncPaths.TryGetValue(sync, out string? previousPath) && previousPath != sync.LocalPath)
                {
                    Logger.Log(Logger.Level.Info, $"Reverting sync path for drive '{sync.Drive.Name}' from '{sync.LocalPath}' to '{previousPath}'");
                    sync.LocalPath = previousPath;
                }
            }
            _previousSyncPaths.Clear();
        }

        private async void AdvancedSettingsButton_Click(object sender, RoutedEventArgs e)
        {
            await AdvancedSettingsDialog.ShowAsync();
        }

        private async void ChangeSyncPathButton_Click(object sender, RoutedEventArgs e)
        {
            Logger.Log(Logger.Level.Info, "Change sync path button clicked, opening folder picker");

            //disable the button to avoid double-clicking
            var senderButton = sender as Button;
            if (senderButton != null)
                senderButton.IsEnabled = false;

            // Create a folder picker
            FolderPicker openPicker = new Windows.Storage.Pickers.FolderPicker();
            var window = ((App)Application.Current)?.CurrentWindow;
            var hWnd = WinRT.Interop.WindowNative.GetWindowHandle(window);
            WinRT.Interop.InitializeWithWindow.Initialize(openPicker, hWnd);
            openPicker.SuggestedStartLocation = PickerLocationId.DocumentsLibrary;
            openPicker.FileTypeFilter.Add("*");
            Windows.Storage.StorageFolder folder = await openPicker.PickSingleFolderAsync();
            if (folder != null)
            {
                Logger.Log(Logger.Level.Info, "Folder picked: " + folder.Path);
                if (!Utility.CheckSyncPathValidity(folder.Path, out string errorMessage))
                {
                    Logger.Log(Logger.Level.Warning, $"Selected folder path '{folder.Path}' is not valid for syncing: {errorMessage}");
                    FolderSelectionError.IsOpen = true;
                    if (senderButton != null)
                        senderButton.IsEnabled = true;
                    return;
                }
                if (_onBoardingViewModel == null)
                {
                    Logger.Log(Logger.Level.Error, "OnBoardingViewModel is null in ChangeSyncPathButton_Click");
                    if (senderButton != null)
                        senderButton.IsEnabled = true;
                    return;
                }
                if (_onBoardingViewModel.NewSyncs.Any(s => s.LocalPath.Equals(folder.Path, StringComparison.OrdinalIgnoreCase)))
                {
                    Logger.Log(Logger.Level.Warning, $"Selected folder path '{folder.Path}' is already used by another sync.");
                    FolderSelectionError.IsOpen = true;
                    if (senderButton != null)
                        senderButton.IsEnabled = true;
                    return;
                }

                if (senderButton?.DataContext is Sync sync && _onBoardingViewModel != null)
                {
                    sync.LocalPath = folder.Path;
                    sync.SyncType = Utility.SupportOnlineSync(folder.Path) ? SyncType.Online : SyncType.Offline;
                    Logger.Log(Logger.Level.Info, $"Sync path for drive '{sync.Drive.Name}' updated to '{sync.LocalPath}' with sync type '{sync.SyncType}'");
                    RefreshAdvancedSettingsConfirmButtonIsEnabled();
                }
                else
                {
                    Logger.Log(Logger.Level.Error, "ChangeSyncPathButton_Click: DataContext is not a Sync or OnBoardingViewModel is null");
                }
            }
            else
            {
                Logger.Log(Logger.Level.Info, "Operation cancelled - No folder was picked");
            }

            if (senderButton != null)
                senderButton.IsEnabled = true;
        }

        private void AdvancedSettingsDialog_Opened(ContentDialog sender, ContentDialogOpenedEventArgs args)
        {
            _previousSyncPaths.Clear();
            if (_onBoardingViewModel == null)
            {
                Logger.Log(Logger.Level.Error, "OnBoardingViewModel is null in AdvancedSettingsDialog_Opened");
                return;
            }
            // Store the current paths to allow reverting if needed
            foreach (var sync in _onBoardingViewModel.NewSyncs)
            {
                _previousSyncPaths[sync] = sync.LocalPath;
            }
        }

        private void RefreshAdvancedSettingsConfirmButtonIsEnabled()
        {
            if (_onBoardingViewModel == null)
            {
                Logger.Log(Logger.Level.Error, "OnBoardingViewModel is null");
                return;
            }
            // Ensure at least one sync path has changed to enable the confirm button
            var syncsWithChangedPaths = _onBoardingViewModel.NewSyncs.Where(s => _previousSyncPaths.ContainsKey(s) && _previousSyncPaths[s] != s.LocalPath).ToList();
            AdvancedSettingsDialog.IsPrimaryButtonEnabled = syncsWithChangedPaths.Count > 0;
        }

        private void Finish_Click(object sender, RoutedEventArgs e)
        {
            Frame.Navigate(typeof(FinishingPage), _onBoardingViewModel);
        }

        private async void StartFree_Click(object sender, RoutedEventArgs e)
        {
            var resourceLoader = Windows.ApplicationModel.Resources.ResourceLoader.GetForViewIndependentUse();
            await Windows.System.Launcher.LaunchUriAsync(new Uri(resourceLoader.GetString("Global_kDriveOffersUrl")));
        }

        private async void OffersButton_Click(object sender, RoutedEventArgs e)
        {
            var resourceLoader = Windows.ApplicationModel.Resources.ResourceLoader.GetForViewIndependentUse();
            await Windows.System.Launcher.LaunchUriAsync(new Uri(resourceLoader.GetString("Global_kDriveOffersUrl")));
        }
    }
}
