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
using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.Pages.Settings
{

    public sealed partial class DriveManagementPage : Page
    {
        private readonly IAnalyticsService _analyticsService = App.ServiceProvider.GetRequiredService<IAnalyticsService>();
        private readonly AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        public AppModel ViewModel { get { return _viewModel; } }
        public Drive? ManagedDrive { get; set; }
        public IDrive? BaseDrive { get; set; }

        public DriveManagementPage()
        {
            Logger.LogInfo("Navigated to DriveManagementPage - Initializing DriveManagementPage components");
            InitializeComponent();
            SetupNavBar("");
            Logger.LogDebug("DriveManagementPage components initialized");
        }
        protected override void OnNavigatedTo(NavigationEventArgs e)
        {
            BaseDrive = e.Parameter as IDrive;
            if (BaseDrive is null)
            {
                var errorMessage = "Drive parameter missing when navigating to DriveManagementPage";
                Logger.LogError(errorMessage, "DriveManagementPage: Missing drive parameter");
                AppModel.UIThreadDispatcher.TryEnqueue(() => { Frame.GoBack(); }); // Frame.GoBack() must be called outside of OnNavigatedTo
                return;
            }
            SetupNavBar(BaseDrive.Name);

            if (BaseDrive is ViewModels.Drive drive)
            {
                if (!ViewModel.AllDrives.Contains(drive))
                {
                    // Can happen if a user uses the back button after being redirected to the settings page following a drive deletion.
                    AppModel.UIThreadDispatcher.TryEnqueue(() => { Frame.GoBack(); }); // Frame.GoBack() must be called outside of OnNavigatedTo
                    return;
                }

                ManagedDrive = drive;
            }
            else if (ViewModel.AllDrives.Any(d => d.DriveId == BaseDrive.DriveId && d.AccountId == BaseDrive.AccountId && d.UserDbId == BaseDrive.UserDbId))
            {
                // Can happen if a user uses the back button after setting up a new drive.
                Logger.LogInfo("The Available drive have an equivalent configured drive that should be used");
                AppModel.UIThreadDispatcher.TryEnqueue(() => { Frame.GoBack(); }); // Frame.GoBack() must be called outside of OnNavigatedTo
                return;
            }
            _analyticsService.TrackPageView(Analytics.Keys.Category.DriveManagementPage);
        }
        protected async override void OnNavigatedFrom(NavigationEventArgs e)
        {
            await SyncExclusionSelector.DisposeAsync();
        }

        private void SetupNavBar(string driveName)
        {
            NavBar.ItemsSource = new string[] { Localizer.Instance.GetString("settingsTitle"), driveName };
        }

        private void NavBar_ItemClicked(BreadcrumbBar sender, BreadcrumbBarItemClickedEventArgs args)
        {
            if (args.Index == 0)
            {
                _analyticsService.TrackClick(Analytics.Keys.Category.DriveManagementPage, Analytics.Keys.EventName.SettingsBreadcrumbs);
                Logger.LogDebug("Navigating to SettingsPage");
                Frame.Navigate(typeof(SettingsPage));
            }
        }

        private async void LocationHyperLinkButton_Clicked(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            string? path = ManagedDrive?.MainSync?.LocalPath ?? null;
            if (path is null)
            {
                Logger.LogError("Cannot open local folder: MainSync or LocalPath is null");
                return;
            }
            _analyticsService.TrackClick(Analytics.Keys.Category.DriveManagementPage, Analytics.Keys.EventName.OpenSyncDir);
            await Utility.OpenFolderSecurely(path);
        }

        private async void SyncTypeRadioButton_Click(object sender, RoutedEventArgs e)
        {
            var radioButton = sender as RadioButton;
            if (radioButton is null)
            {
                Logger.LogError("Sender of SyncTypeRadioButton_Click is not a RadioButton");
                return;
            }

            if (!radioButton.IsEnabled || radioButton.IsChecked != true)
                return;

            Sync? sync = ManagedDrive?.MainSync;
            if (sync is null)
            {
                Logger.LogError("Could not get sync from ManagedDrive?.MainSync when clicking on sync mode radio button");
                return;
            }

            bool targetOnline = radioButton.Name == "OnlineRadioButton";
            bool targetOffline = radioButton.Name == "OfflineRadioButton";

            if (!targetOffline && !targetOnline)
            {
                Logger.LogError("Unknown radio button name for sync mode change");
                return;
            }

            if (targetOffline && sync.SyncType == Types.SyncType.Offline)
            {
                Logger.LogInfo("User clicked on Offline sync mode radio button while already in Offline mode");
                return;
            }

            if (targetOnline && sync.SyncType == Types.SyncType.Online)
            {
                Logger.LogInfo("User clicked on Online sync mode radio button while already in Online mode");
                return;
            }

            ContentDialog dialog = new ContentDialog
            {
                XamlRoot = XamlRoot,
                Title = Localizer.Instance.GetString("dialogSyncModeChangeWarningTitle"),
                PrimaryButtonText = targetOnline ? Localizer.Instance.GetString("buttonChangeToOnline") : Localizer.Instance.GetString("buttonChangeToOffline"),
                CloseButtonText = Localizer.Instance.GetString("buttonCancel"),
                DefaultButton = ContentDialogButton.Close,
                Content = Localizer.Instance.GetString("dialogSyncModeChangeWarningContent")
            };

            bool canceledByUser = await dialog.ShowAsync() != ContentDialogResult.Primary;
            if (canceledByUser)
            {
                _analyticsService.TrackClick(Analytics.Keys.Category.DriveManagementPage, Analytics.Keys.EventName.CancelSyncModeSwitch);
                // This is needed to revert the radio button state back to offline, as changing the sync type to online can fail and we want to reflect that in the UI.
                if (targetOnline)
                {
                    Logger.LogInfo("User canceled the change to online Sync mode");
                    sync.SyncType = Types.SyncType.Online;
                    sync.SyncType = Types.SyncType.Offline; // Force all the bindings to update, especially the one on the radio buttons.IsChecked
                }
                else
                {
                    Logger.LogInfo("User canceled the change to offline Sync mode");
                    sync.SyncType = Types.SyncType.Offline;
                    sync.SyncType = Types.SyncType.Online; // Force all the bindings to update, especially the one on the radio buttons.IsChecked
                }
                return;
            }


            _analyticsService.TrackClick(Analytics.Keys.Category.DriveManagementPage, Analytics.Keys.EventName.ConfirmSyncModeSwitch, targetOnline ? 1 : 0);

            bool success = false;
            if (targetOnline)
                success = await sync.ChangeSyncType(Types.SyncType.Online);
            else
                success = await sync.ChangeSyncType(Types.SyncType.Offline);

            if (!success)
            {
                ContentDialog errorDialog = new ContentDialog
                {
                    XamlRoot = XamlRoot,
                    Title = Localizer.Instance.GetString("dialogSyncModeChangeErrorTitle"),
                    CloseButtonText = Localizer.Instance.GetString("buttonCancel"),
                    Content = Localizer.Instance.GetString("dialogSyncModeChangeErrorContent")
                };
                await errorDialog.ShowAsync();
            }
        }

        private void FixForegroundOnPointerExited(object sender, Microsoft.UI.Xaml.Input.PointerRoutedEventArgs e)
        {
            if (sender is Control control)
            {
                var currentForeground = control.Foreground;
                control.Foreground = null;
                control.Foreground = currentForeground;
            }
        }

        private async void RemoveSyncSettingsCard_Click(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            if (ManagedDrive is null || ManagedDrive.MainSync is null)
            {
                Logger.LogError("Cannot remove sync: ManagedDrive or MainSync is null");
                return;
            }

            var control = sender as Control;
            if (control is null)
            {
                Logger.LogError("Cannot remove sync: sender is not a Control");
                return;
            }

            control.IsEnabled = false;
            bool goBackOnceDone = ManagedDrive.Syncs.Count() == 1; // If we are removing the last sync of the drive, go back to settings page once done.

            ContentDialog dialog = new ContentDialog
            {
                XamlRoot = XamlRoot,
                Title = Localizer.Instance.GetString("dialogSyncDeletionWarningTitle"),
                PrimaryButtonText = Localizer.Instance.GetString("buttonRemove"),
                CloseButtonText = Localizer.Instance.GetString("buttonCancel"),
                DefaultButton = ContentDialogButton.Close,
                Content = Localizer.Instance.GetString("dialogSyncDeletionWarningContent")
            };

            var dialogResult = await dialog.ShowAsync();
            if (dialogResult != ContentDialogResult.Primary)
            {
                Logger.LogInfo("User canceled sync removal");
                control.IsEnabled = true;
                return;
            }

            Logger.LogInfo("User confirmed sync removal");
            _analyticsService.TrackClick(Analytics.Keys.Category.DriveManagementPage, Analytics.Keys.EventName.Delete);
            if (!await ManagedDrive.RemoveSync(ManagedDrive.MainSync, CancellationToken.None))
            {
                Logger.LogError("Failed to remove sync");
                Utility.ShowUnexpectedErrorTeachingTip();
                control.IsEnabled = true;
                return;
            }
            Logger.LogInfo("Sync removed successfully");
            if (goBackOnceDone)
            {
                Frame.Navigate(typeof(SettingsPage));
            }

            control.IsEnabled = true;
        }

        private async void SetupMainSyncButton_Click(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            if (BaseDrive is null)
            {
                Logger.LogError("Cannot setup main sync: BaseDrive is null");
                return;
            }

            Control? control = sender as Control;
            if (control is not null)
                control.IsEnabled = false;

            var commServices = App.ServiceProvider.GetRequiredService<IServerCommService>();
            string? result = await commServices.GetGoodPathForNewSync(BaseDrive, CancellationToken.None);
            if (result is null)
            {
                Logger.LogError($"Failed to get a valid sync path for drive '{BaseDrive.Name}'",
                    "DriveManagementPage: Failed to get valid sync path");
                Utility.ShowUnexpectedErrorTeachingTip();
                if (control is not null)
                    control.IsEnabled = true;
                return;
            }

            NewSync newSync = new(BaseDrive) { DefaultPath = result, LocalPath = result };
            await newSync.SelectBestVfsMode();
            List<NewSync> newSyncs = [newSync];

            CustomControls.DriveSetupContentDialog dialog = new(this.XamlRoot, newSyncs);
            await dialog.ShowAsync();

            if (dialog.Result == CustomControls.DriveSetupContentDialog.DriveSetupResult.Cancelled)
            {
                Logger.LogInfo($"User canceled main sync setup for drive '{BaseDrive.Name}'");
                if (control is not null)
                    control.IsEnabled = true;
                return;
            }

            var commService = App.ServiceProvider.GetRequiredService<IServerCommService>();

            _analyticsService.TrackClick(Analytics.Keys.Category.DriveManagementPage, Analytics.Keys.EventName.Create);
            Logger.LogDebug($"Setting up new sync: LocalPath={newSync.LocalPath}, RemotePath={newSync.RemotePath}, Drive={newSync.Drive.Name}");
            if (!await commService.AddSync(newSync, CancellationToken.None))
            {
                Logger.LogError($"Failed to add new sync for drive '{BaseDrive.Name}'",
                    "DriveManagementPage: Failed to add sync");
                if (control is not null)
                    control.IsEnabled = true;
                Utility.ShowUnexpectedErrorTeachingTip();
                return;
            }

            if (ManagedDrive is null) // if the drive was not configured before, set it up now
            {
                Drive? drive = ViewModel.AllDrives.FirstOrDefault(d => d?.DriveId == BaseDrive.DriveId && d?.AccountId == BaseDrive.AccountId && d?.UserDbId == BaseDrive.UserDbId, null);
                int count = 100;
                while (drive is null && count > 0)
                {
                    await Task.Delay(100);
                    drive = ViewModel.AllDrives.FirstOrDefault(d => d?.DriveId == BaseDrive.DriveId && d?.AccountId == BaseDrive.AccountId && d?.UserDbId == BaseDrive.UserDbId, null);
                    count--;
                }

                if (drive is not null)
                {
                    ManagedDrive = drive;
                    BaseDrive = drive;
                    Bindings.Update();
                }
                else
                {
                    Logger.LogError($"Drive '{BaseDrive.Name}' was not found in AllDrives after sync setup",
                        "DriveManagementPage: Drive not found after sync setup");
                }
            }

            if (control is not null)
                control.IsEnabled = true;

        }

        private void AdvancedSyncsSettingsCard_Click(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            _analyticsService.TrackClick(Analytics.Keys.Category.DriveManagementPage, Analytics.Keys.EventName.ManageAdvancedSync);
            Frame.Navigate(typeof(DriveAdvancedSyncsPage), BaseDrive);
        }

        private void ExclusionsSettingsExpander_Expanded(object sender, EventArgs e)
        {
            _analyticsService.TrackClick(Analytics.Keys.Category.DriveManagementPage, Analytics.Keys.EventName.ShowItemExclusion);
        }
    }
}
