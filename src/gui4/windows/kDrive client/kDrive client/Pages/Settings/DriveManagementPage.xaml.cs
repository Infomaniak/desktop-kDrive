using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.Linq;
using System.Threading;

namespace Infomaniak.kDrive.Pages.Settings
{
    public sealed partial class DriveManagementPage : Page
    {
        private AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        public AppModel ViewModel { get { return _viewModel; } }
        public Drive? ManagedDrive { get; set; }

        public DriveManagementPage()
        {
            Logger.Log(Logger.Level.Info, "Navigated to DriveManagementPage - Initializing DriveManagementPage components");
            InitializeComponent();
            SetupNavBar(null);
            Logger.Log(Logger.Level.Debug, "DriveManagementPage components initialized");
        }

        protected override void OnNavigatedTo(NavigationEventArgs e)
        {
            if (e.Parameter is ViewModels.Drive drive)
            {
                if (!ViewModel.AllDrives.Contains(drive))
                {
                    // Can happen if a user uses the back button after being redirected to the settings page following a drive deletion.
                    AppModel.UIThreadDispatcher.TryEnqueue(() => { Frame.GoBack(); }); // Frame.GoBack() must be called outside of OnNavigatedTo
                    return;
                }

                ManagedDrive = drive;
                SetupNavBar(ManagedDrive.Name);
            }
            else
            {
                var errorMessage = "Drive parameter missing when navigating to DriveManagementPage";
                Logger.Log(Logger.Level.Error, errorMessage);
                AppModel.UIThreadDispatcher.TryEnqueue(() => { Frame.GoBack(); }); // Frame.GoBack() must be called outside of OnNavigatedTo
            }
        }

        private void SetupNavBar(string? driveName)
        {
            if (driveName is not null)
            {
                NavBar.ItemsSource = new string[] { Utility.GetLocalizedString("Page_SettingsPage_Title/Text"), Utility.GetLocalizedString("Page_DriveManagement_Title/Text"), driveName };
            }
            else
            {
                NavBar.ItemsSource = new string[] { Utility.GetLocalizedString("Page_SettingsPage_Title/Text"), Utility.GetLocalizedString("Page_DriveManagement_Title/Text") };
            }
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

        private void LocationHyperLinkButton_Clicked(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            string? path = ManagedDrive?.MainSync?.LocalPath ?? null;
            if (path is null)
            {
                Logger.Log(Logger.Level.Error, "Cannot open local folder: MainSync or LocalPath is null");
                return;
            }
            Utility.OpenFolderSecurely(path);
        }

        private async void LiteSyncRadioButtonChecked(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            if (!IsLoaded || !OnlineRadioButton.IsEnabled)
                return;
            OnlineRadioButton.IsEnabled = false;
            OfflineRadioButton.IsEnabled = false;
            bool succeed = false;
            bool canceled = false;
            if (await Utility.ShowContentDialogAsync(this.XamlRoot, "Page_Settings_DriveManagementPage_SyncMode_WarningDialog") == ContentDialogResult.Primary)
            {
                Logger.Log(Logger.Level.Info, "User confirmed to change to online Sync mode");
                if (ManagedDrive?.MainSync is not null)
                {
                    succeed = await ManagedDrive.MainSync.ChangeSyncType(Types.SyncType.Online);
                }
            }
            else
            {
                canceled = true;
            }

            if (!succeed)
            {
                if (!canceled)
                {
                    Logger.Log(Logger.Level.Info, "User canceled the change to online Sync mode");
                    await Utility.ShowContentDialogAsync(this.XamlRoot, "Page_Settings_DriveManagementPage_SyncMode_ErrorDialog");
                }
                OnlineRadioButton.IsChecked = false;
                OfflineRadioButton.IsChecked = true;

            }
            OnlineRadioButton.IsEnabled = true;
            OfflineRadioButton.IsEnabled = true;
        }

        private async void LiteSyncRadioButtonUnchecked(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            if (!IsLoaded || !OnlineRadioButton.IsEnabled)
                return;
            OnlineRadioButton.IsEnabled = false;
            OfflineRadioButton.IsEnabled = false;
            bool succeed = false;
            bool canceled = false;

            if (await Utility.ShowContentDialogAsync(this.XamlRoot, "Page_Settings_DriveManagementPage_SyncMode_WarningDialog") == ContentDialogResult.Primary)
            {
                Logger.Log(Logger.Level.Info, "User confirmed to change to offline Sync mode");
                if (ManagedDrive?.MainSync is not null)
                {
                    succeed = await ManagedDrive.MainSync.ChangeSyncType(Types.SyncType.Offline);
                }
            }
            else
            {
                canceled = true;
            }

            if (!succeed)
            {
                if (!canceled)
                {
                    Logger.Log(Logger.Level.Info, "User canceled the change to online Sync mode");
                    await Utility.ShowContentDialogAsync(this.XamlRoot, "Page_Settings_DriveManagementPage_SyncMode_ErrorDialog");
                }
                OnlineRadioButton.IsChecked = true;
                OfflineRadioButton.IsChecked = false;
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

        private async void SaveSyncSelection_click(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            Control? control = sender as Control;
            if (control is not null)
                control.IsEnabled = false;
            await ExclSelector.SaveChanges();
            if (control is not null)
                control.IsEnabled = true;

        }

        private async void CancelSyncSelection_click(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            Control? control = sender as Control;
            if (control is not null)
                control.IsEnabled = false;
            await ExclSelector.CancelChanges();
            if (control is not null)
                control.IsEnabled = true;
        }
    }
}
