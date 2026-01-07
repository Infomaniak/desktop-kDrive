using Infomaniak.kDrive.OnBoarding;
using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Windows.ApplicationModel.VoiceCommands;
using Windows.Storage.Pickers;

namespace Infomaniak.kDrive.Pages.Onboarding
{
    public sealed partial class DriveSelectionPage : Page
    {
        private readonly AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        private ViewModels.Onboarding? _onBoardingViewModel;
        private readonly Dictionary<NewSync, string> _previousSyncPaths = []; // To store previous sync paths and allow reverting if needed in advanced settings
        public AppModel ViewModel { get { return _viewModel; } }
        public ViewModels.Onboarding? ObViewModel { get => _onBoardingViewModel; }
        public DriveSelectionPage()
        {
            Logger.Log(Logger.Level.Info, "Navigated to DriveSelectionPage - Initializing DriveSelectionPage components");
            InitializeComponent();
            Logger.Log(Logger.Level.Debug, "DriveSelectionPage components initialized");
        }

        protected override async void OnNavigatedTo(NavigationEventArgs e)
        {
            if (e.Parameter is ViewModels.Onboarding obvm)
            {
                _onBoardingViewModel = obvm;
                if (ObViewModel?.SelectedUser is null)
                {
                    Logger.Log(Logger.Level.Error, "SelectedUser is null in OnBoardingViewModel when navigating to DriveSelectionPage");
                    Frame.GoBack();
                    return;
                }
                await ObViewModel.SelectedUser.RefreshAvailableDrives();
                if (!ObViewModel.SelectedUser.AllDrives.Any())
                {
                    Logger.Log(Logger.Level.Info, "No drives found for user in DriveSelectionPage - Navigating to NoDrivePage");
                    Frame.Navigate(typeof(NoDrivesPage), ObViewModel);
                    return;
                }
                if (App.Current is App { CurrentWindow: OnBoardingWindow onBoardingWindow })
                    onBoardingWindow.UpdateLottieSource("Infomaniak.Custom.Animations.synchro-file", 219);
            }
            else
            {
                Logger.Log(Logger.Level.Fatal, "OnBoardingViewModel parameter missing when navigating to DriveSelectionPage");
                throw new Exception("OnBoardingViewModel parameter missing when navigating to DriveSelectionPage");
            }
        }

        private async void DriveListCheckBox_Checked(object sender, RoutedEventArgs e)
        {
            if (sender is CheckBox cb && cb.DataContext is IDrive drive && _onBoardingViewModel != null)
            {
                var commServices = App.ServiceProvider.GetRequiredService<IServerCommService>();
                string userProfile = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
                string desiredFolderName = drive.Name.StartsWith("kDrive") ? drive.Name : $"kDrive {drive.Name}";
                string desiredPath = Path.Combine(userProfile, desiredFolderName);
                IServerCommService.GetGoodPathResult? result = await commServices.GetGoodPathForNewSync(drive, desiredPath, CancellationToken.None);
                if (!result.HasValue || result.Value.GoodPath is null)
                {
                    Logger.Log(Logger.Level.Error, $"Failed to get a valid sync path for drive '{drive.Name}': {result?.ErrorMessage ?? "Unknown error"}");
                    return;
                }

                NewSync newSync = new() { Drive = drive };
                await SetNewSyncLocalPathAndUpdateVfsMode(newSync, result.Value.GoodPath);

                _onBoardingViewModel.NewSyncs.Add(newSync);
            }
        }

        private void DriveListCheckBox_Unchecked(object sender, RoutedEventArgs e)
        {
            if (sender is CheckBox cb && cb.DataContext is IDrive drive && _onBoardingViewModel != null)
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

        private async void AdvancedSettingsDialog_CloseButtonClick(ContentDialog sender, ContentDialogButtonClickEventArgs args)
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
                    Logger.Log(Logger.Level.Info, $"Reverting sync path for drive '{sync.Drive?.Name ?? "unknown"}' from '{sync.LocalPath}' to '{previousPath}'");
                    await SetNewSyncLocalPathAndUpdateVfsMode(sync, previousPath);
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

            NewSync? newSync = senderButton?.DataContext as NewSync;
            if (newSync is null)
            {
                Logger.Log(Logger.Level.Error, "ChangeSyncPathButton_Click: DataContext is not a NewSync");
                if (senderButton != null)
                    senderButton.IsEnabled = true;
                return;
            }

            // Create a folder picker
            FolderPicker openPicker = new();
            var window = ((App)Application.Current)?.CurrentWindow;
            var hWnd = WinRT.Interop.WindowNative.GetWindowHandle(window);
            WinRT.Interop.InitializeWithWindow.Initialize(openPicker, hWnd);
            openPicker.SuggestedStartLocation = PickerLocationId.DocumentsLibrary;
            openPicker.FileTypeFilter.Add("*");
            Windows.Storage.StorageFolder folder = await openPicker.PickSingleFolderAsync();
            if (folder != null)
            {
                Logger.Log(Logger.Level.Info, "Folder picked: " + folder.Path);
                if (_onBoardingViewModel is null)
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

                var commServices = App.ServiceProvider.GetRequiredService<IServerCommService>();
                bool? result = await commServices.IsPathValidForNewSync(folder.Path, CancellationToken.None);
                if (result is null || result == false)
                {
                    Logger.Log(Logger.Level.Warning, $"Selected folder path '{folder.Path}' is not valid for syncing");
                    FolderSelectionError.IsOpen = true;
                    if (senderButton != null)
                        senderButton.IsEnabled = true;
                    return;
                }

                await SetNewSyncLocalPathAndUpdateVfsMode(newSync, folder.Path);
                Logger.Log(Logger.Level.Info, $"Sync path for drive '{newSync!.Drive?.Name ?? "unknown"}' updated to '{newSync!.LocalPath}' with sync type '{newSync.SyncType}'");
                RefreshAdvancedSettingsConfirmButtonIsEnabled();
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
            AdvancedSettingsDialog.IsPrimaryButtonEnabled = _onBoardingViewModel.NewSyncs.Where(s => _previousSyncPaths.ContainsKey(s) && _previousSyncPaths[s] != s.LocalPath).Any();
        }

        private void Finish_Click(object sender, RoutedEventArgs e)
        {
            Frame.Navigate(typeof(FinishingPage), _onBoardingViewModel);
        }

        private async Task SetNewSyncLocalPathAndUpdateVfsMode(NewSync sync, string newLocalPath)
        {
            var commServices = App.ServiceProvider.GetRequiredService<IServerCommService>();
            bool? supportOnlineMode = await commServices.PathSupportLiteSync(newLocalPath, CancellationToken.None);
            if (supportOnlineMode is null)
                Logger.Log(Logger.Level.Warning, $"Could not determine if the path '{newLocalPath}' supports online mode. Defaulting to offline sync.");

            sync.LocalPath = newLocalPath;
            sync.SyncType = (supportOnlineMode ?? false) ? SyncType.Online : SyncType.Offline;
        }
    }

    public partial class DriveTemplateSelector : DataTemplateSelector
    {
        public DataTemplate? SingleAccountDriveTemplate { get; set; }
        public DataTemplate? MultiAccountDriveTemplate { get; set; }

        protected override DataTemplate? SelectTemplateCore(object item, DependencyObject container)
        {
            if (item is not IDrive drive)
                return base.SelectTemplateCore(item, container);

            User? user = App.ServiceProvider.GetRequiredService<AppModel>().Users.FirstOrDefault(u => u.DbId == drive.UserDbId);
            if (user is null)
            {
                Logger.Log(Logger.Level.Warning, "DriveTemplateSelector: User not found for drive");
                return SingleAccountDriveTemplate; // Fallback to single account template
            }

            return user.AllDrives.Select(drive => drive.AccountId).Distinct().Count() > 1 ? MultiAccountDriveTemplate : SingleAccountDriveTemplate;
        }
    }
}
