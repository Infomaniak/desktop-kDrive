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
        private readonly AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        public AppModel ViewModel { get { return _viewModel; } }
        public Drive? ManagedDrive { get; set; }
        public IDrive? BaseDrive { get; set; }

        public DriveManagementPage()
        {
            Logger.Log(Logger.Level.Info, "Navigated to DriveManagementPage - Initializing DriveManagementPage components");
            InitializeComponent();
            SetupNavBar("");
            Logger.Log(Logger.Level.Debug, "DriveManagementPage components initialized");
        }

        protected override void OnNavigatedTo(NavigationEventArgs e)
        {
            BaseDrive = e.Parameter as IDrive;
            if (BaseDrive is null)
            {
                var errorMessage = "Drive parameter missing when navigating to DriveManagementPage";
                Logger.Log(Logger.Level.Error, errorMessage);
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
            else if (ViewModel.AllDrives.FirstOrDefault(d => d.DriveId == BaseDrive.DriveId && d.AccountId == BaseDrive.AccountId && d.UserDbId == BaseDrive.UserDbId, null) is not null)
            {
                // Can happen if a user uses the back button after setting up a new drive.
                Logger.Log(Logger.Level.Info, "The Available drive have an equivalent configured drive that should be used");
                AppModel.UIThreadDispatcher.TryEnqueue(() => { Frame.GoBack(); }); // Frame.GoBack() must be called outside of OnNavigatedTo
                return;
            }
        }

        private void SetupNavBar(string driveName)
        {
            NavBar.ItemsSource = new string[] { Localizer.Instance.GetString("settingsTitle"), driveName };
        }

        private void NavBar_ItemClicked(BreadcrumbBar sender, BreadcrumbBarItemClickedEventArgs args)
        {
            if (args.Index == 0)
            {
                Logger.Log(Logger.Level.Debug, "Navigating to SettingsPage");
                Frame.Navigate(typeof(SettingsPage));
            }
        }

        private async void LocationHyperLinkButton_Clicked(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            string? path = ManagedDrive?.MainSync?.LocalPath ?? null;
            if (path is null)
            {
                Logger.Log(Logger.Level.Error, "Cannot open local folder: MainSync or LocalPath is null");
                return;
            }
            await Utility.OpenFolderSecurely(path);
        }

        private async void SyncTypeRadioButton_Click(object sender, RoutedEventArgs e)
        {
            var radioButton = sender as RadioButton;
            if (radioButton is null)
            {
                Logger.Log(Logger.Level.Error, "Online sync mode radio button is null when clicking on online sync mode radio button");
                return;
            }

            if (!radioButton.IsEnabled || radioButton.IsChecked != true)
                return;

            Sync? sync = ManagedDrive?.MainSync;
            if (sync is null)
            {
                Logger.Log(Logger.Level.Error, "Could not get sync from ManagedDrive?.MainSync when clicking on online sync mode radio button");
                return;
            }

            bool canceledByUser = await Utility.ShowContentDialogAsync(this.XamlRoot, "dialogSyncModeChangeWarning") == ContentDialogResult.Primary;
            if (canceledByUser)
            {
                Logger.Log(Logger.Level.Info, "User canceled the change to online Sync mode");
                // This is needed to revert the radio button state back to offline, as changing the sync type to online can fail and we want to reflect that in the UI.
                if (radioButton.Name == "OnlineRadioButton")
                {
                    sync.SyncType = Types.SyncType.Online;
                    sync.SyncType = Types.SyncType.Offline;
                }
                else
                {
                    sync.SyncType = Types.SyncType.Offline;
                    sync.SyncType = Types.SyncType.Online;
                }
                return;
            }


            bool success = false;
            if (radioButton.Name == "OnlineRadioButton")
                success = await sync.ChangeSyncType(Types.SyncType.Online);
            else if (radioButton.Name == "OfflineRadioButton")
                success = await sync.ChangeSyncType(Types.SyncType.Offline);

            if (!success)
                await Utility.ShowContentDialogAsync(this.XamlRoot, "dialogSyncModeChangeError");
        }

        private void FixForegroundOnPointerExited(object sender, Microsoft.UI.Xaml.Input.PointerRoutedEventArgs e)
        {
            if (sender is Control control)
            {
                var curentForeground = control.Foreground;
                control.Foreground = null;
                control.Foreground = curentForeground;
            }
        }

        private async void RemoveSyncSettingsCard_Click(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            if (ManagedDrive is null || ManagedDrive.MainSync is null)
            {
                Logger.Log(Logger.Level.Error, "Cannot remove sync: ManagedDrive or MainSync is null");
                return;
            }

            var control = sender as Control;
            if (control is null)
            {
                Logger.Log(Logger.Level.Error, "Cannot remove sync: sender is not a Control");
                return;
            }

            control.IsEnabled = false;
            bool goBackOnceDone = ManagedDrive.Syncs.Count() == 1; // If we are removing the last sync of the drive, go back to settings page once done.

            var dialogResult = await Utility.ShowContentDialogAsync(this.XamlRoot, "dialogSyncDeletionWarning");
            if (dialogResult != ContentDialogResult.Primary)
            {
                Logger.Log(Logger.Level.Info, "User canceled sync removal");
                control.IsEnabled = true;
                return;
            }

            Logger.Log(Logger.Level.Info, "User confirmed sync removal");
            if (!await ManagedDrive.RemoveSync(ManagedDrive.MainSync, CancellationToken.None))
            {
                Logger.Log(Logger.Level.Error, "Failed to remove sync");
                Utility.ShowUnexpectedErrorTeachingTip();
                control.IsEnabled = true;
                return;
            }
            Logger.Log(Logger.Level.Info, "Sync removed successfully");
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
                Logger.Log(Logger.Level.Error, "Cannot setup main sync: BaseDrive is null");
                return;
            }

            Control? control = sender as Control;
            if (control is not null)
                control.IsEnabled = false;

            var commServices = App.ServiceProvider.GetRequiredService<IServerCommService>();
            string userProfile = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
            string desiredFolderName = BaseDrive.Name.StartsWith("kDrive") ? BaseDrive.Name : $"kDrive {BaseDrive.Name}";
            string desiredPath = Path.Combine(userProfile, desiredFolderName);
            string? result = await commServices.GetGoodPathForNewSync(BaseDrive, desiredPath, CancellationToken.None);
            if (result is null)
            {
                Logger.Log(Logger.Level.Error, $"Failed to get a valid sync path for drive '{BaseDrive.Name}'");
                Utility.ShowTeachingTipFromxUid("InvalidDefaultSyncLocationTeachingTip");
                return;
            }

            NewSync newSync = new() { Drive = BaseDrive, DefaultPath = result, LocalPath = result };
            await newSync.SelectBestVfsMode();
            List<NewSync> newSyncs = [newSync];

            CustomControls.DriveSetupContentDialog dialog = new(this.XamlRoot, newSyncs);
            await dialog.ShowAsync();

            if (dialog.Result == CustomControls.DriveSetupContentDialog.DriveSetupResult.Cancelled)
            {
                Logger.Log(Logger.Level.Info, $"User canceled main sync setup for drive '{BaseDrive.Name}'");
                if (control is not null)
                    control.IsEnabled = true;
                return;
            }

            var commService = App.ServiceProvider.GetRequiredService<IServerCommService>();

            Logger.Log(Logger.Level.Debug, $"Setting up new sync: LocalPath={newSync.LocalPath}, RemotePath={newSync.RemotePath}, Drive={newSync.Drive.Name}");
            if (!await commService.AddSync(newSync, CancellationToken.None))
            {
                Logger.Log(Logger.Level.Error, $"Failed to add new sync for drive '{BaseDrive.Name}'");
                if (control is not null)
                    control.IsEnabled = true;
                Utility.ShowUnexpectedErrorTeachingTip();
                return;
            }

            if (ManagedDrive is null) // if the drive was not configured before, set it up now
            {
                Drive? drive = ViewModel.AllDrives.FirstOrDefault(d => d.DriveId == BaseDrive.DriveId && d.AccountId == BaseDrive.AccountId && d.UserDbId == BaseDrive.UserDbId, null);
                int count = 100;
                while (drive is null && count > 0)
                {
                    await Task.Delay(100);
                    drive = ViewModel.AllDrives.FirstOrDefault(d => d.DriveId == BaseDrive.DriveId && d.AccountId == BaseDrive.AccountId && d.UserDbId == BaseDrive.UserDbId, null);
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
                    Logger.Log(Logger.Level.Error, $"Drive '{BaseDrive.Name}' was not found in AllDrives after sync setup");
                }
            }

            if (control is not null)
                control.IsEnabled = true;

        }

        private void AdvancedSyncsSettingsCard_Click(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            Frame.Navigate(typeof(DriveAdvancedSyncsPage), BaseDrive);
        }
    }
}
