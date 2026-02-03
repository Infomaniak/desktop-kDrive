using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
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
        private AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
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
            else if (ViewModel.AllDrives.FirstOrDefault(d => (d.DriveId == BaseDrive.DriveId && d.AccountId == BaseDrive.AccountId && d.UserDbId == BaseDrive.UserDbId), null) is not null)
            {
                // Can happen if a user uses the back button after setting up a new drive.
                Logger.Log(Logger.Level.Info, "The Available drive have an equivalent configured drive that should be used");
                AppModel.UIThreadDispatcher.TryEnqueue(() => { Frame.GoBack(); }); // Frame.GoBack() must be called outside of OnNavigatedTo
                return;
            }
        }

        private void SetupNavBar(string driveName)
        {
            NavBar.ItemsSource = new string[] { Utility.GetLocalizedString("Page_SettingsPage_Title/Text"), Utility.GetLocalizedString("Page_DriveManagement_Title/Text"), driveName };
        }

        private void NavBar_ItemClicked(BreadcrumbBar sender, BreadcrumbBarItemClickedEventArgs args)
        {
            if (args.Index == 0)
            {
                Logger.Log(Logger.Level.Debug, "Navigating to SettingsPage");
                Frame.Navigate(typeof(SettingsPage));
            }
            else if (args.Index == 1)
            {
                Logger.Log(Logger.Level.Debug, "Navigating to SettingsPage focused on Account section");
                Frame.Navigate(typeof(SettingsPage)); // TODO: Focus on account section
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
        private void OnlineRadioButtonClicked(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            if (OnlineRadioButton.IsChecked == true)
            {
                OnlineRadioButtonChecked();
            }
            else
            {
                OnlineRadioButtonUnchecked();
            }

        }

        private async void OnlineRadioButtonChecked()
        {
            if (!IsLoaded || !OnlineRadioButton.IsEnabled)
                return;

            Logger.Log(Logger.Level.Info, "User initiated change to online Sync mode");
            OnlineRadioButton.IsEnabled = false;
            OfflineRadioButton.IsEnabled = false;
            bool succeed = false;
            bool canceledByUser = await Utility.ShowContentDialogAsync(this.XamlRoot, "Page_Settings_DriveManagementPage_SyncMode_WarningDialog") == ContentDialogResult.Primary;

            var revertFunc = () =>
            {
                OnlineRadioButton.IsChecked = false;
                OfflineRadioButton.IsChecked = true;
                OnlineRadioButton.IsEnabled = true;
                OfflineRadioButton.IsEnabled = true;
            };

            if (canceledByUser)
            {
                Logger.Log(Logger.Level.Info, "User canceled the change to online Sync mode");
                revertFunc();
                return;
            }

            Logger.Log(Logger.Level.Info, "User confirmed to change to online Sync mode");
            if (ManagedDrive?.MainSync is not null)
            {
                succeed = await ManagedDrive.MainSync.ChangeSyncType(Types.SyncType.Online);
            }
            else
            {
                Logger.Log(Logger.Level.Error, "Cannot change sync mode: ManagedDrive or MainSync is null");
                succeed = false;
            }


            if (!succeed)
            {
                await Utility.ShowContentDialogAsync(this.XamlRoot, "Page_Settings_DriveManagementPage_SyncMode_ErrorDialog");
                revertFunc();
            }

            OnlineRadioButton.IsEnabled = true;
            OfflineRadioButton.IsEnabled = true;
        }

        private async void OnlineRadioButtonUnchecked()
        {
            if (!IsLoaded || !OnlineRadioButton.IsEnabled)
                return;

            Logger.Log(Logger.Level.Info, "User initiated change to offline Sync mode");
            OnlineRadioButton.IsEnabled = false;
            OfflineRadioButton.IsEnabled = false;
            bool succeed = false;
            bool canceledByUser = await Utility.ShowContentDialogAsync(this.XamlRoot, "Page_Settings_DriveManagementPage_SyncMode_WarningDialog") == ContentDialogResult.Primary;

            var revertFunc = () =>
            {
                OnlineRadioButton.IsChecked = true;
                OfflineRadioButton.IsChecked = false;
                OnlineRadioButton.IsEnabled = true;
                OfflineRadioButton.IsEnabled = true;
            };

            if (canceledByUser)
            {
                Logger.Log(Logger.Level.Info, "User canceled the change to online Sync mode");
                revertFunc();
                return;
            }

            Logger.Log(Logger.Level.Info, "User confirmed to change to offline Sync mode");
            if (ManagedDrive?.MainSync is not null)
            {
                succeed = await ManagedDrive.MainSync.ChangeSyncType(Types.SyncType.Offline);
            }
            else
            {
                Logger.Log(Logger.Level.Error, "Cannot change sync mode: ManagedDrive or MainSync is null");
                succeed = false;
            }


            if (!succeed)
            {
                await Utility.ShowContentDialogAsync(this.XamlRoot, "Page_Settings_DriveManagementPage_SyncMode_ErrorDialog");
                revertFunc();
            }

            OnlineRadioButton.IsEnabled = true;
            OfflineRadioButton.IsEnabled = true;
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
            if (ManagedDrive is not null && ManagedDrive.MainSync is not null)
            {
                var control = sender as Control;
                if (control is not null)
                    control.IsEnabled = false;
                bool goBackOnceDone = ManagedDrive.Syncs.Count() == 1; // If we are removing the last sync of the drive, go back to settings page once done.
                var dialogResult = await Utility.ShowContentDialogAsync(this.XamlRoot, "Page_Settings_DriveManagementPage_SyncDeletion_WarningDialog");
                if (dialogResult == ContentDialogResult.Primary)
                {
                    Logger.Log(Logger.Level.Info, "User confirmed sync removal");
                    await ManagedDrive.RemoveSync(ManagedDrive.MainSync, CancellationToken.None);
                    if (goBackOnceDone)
                    {
                        Frame.Navigate(typeof(SettingsPage));
                    }
                }
                else
                {
                    Logger.Log(Logger.Level.Info, "User canceled sync removal");
                }
                if (control is not null)
                    control.IsEnabled = true;
            }
            else
            {
                Logger.Log(Logger.Level.Error, "Cannot remove sync: ManagedDrive or MainSync is null");
            }
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
            IServerCommService.GetGoodPathResult? result = await commServices.GetGoodPathForNewSync(BaseDrive, desiredPath, CancellationToken.None);
            if (!result.HasValue || result.Value.GoodPath is null)
            {
                Logger.Log(Logger.Level.Error, $"Failed to get a valid sync path for drive '{BaseDrive.Name}': {result?.ErrorMessage ?? "Unknown error"}");
                return;
            }

            NewSync newSync = new NewSync()
            {
                Drive = BaseDrive,
                DefaultPath = result.Value.GoodPath,
                LocalPath = result.Value.GoodPath
            };
            await newSync.SelectBestVfsMode();
            List<NewSync> newSyncs = new() { newSync };

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
