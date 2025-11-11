using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.Threading.Tasks;

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
                ManagedDrive = drive;
                SetupNavBar(ManagedDrive.Name);
            }
            else
            {
                var errorMessage = "Drive parameter missing when navigating to DriveManagementPage";
                Logger.Log(Logger.Level.Error, errorMessage);
                Frame.GoBack();
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
            if (!IsLoaded || !OnlineRadioButton.IsEnabled) return;
            OnlineRadioButton.IsEnabled = false;
            OfflineRadioButton.IsEnabled = false;
            if (await ShowConversionDialog() == ContentDialogResult.Primary)
            {
                Logger.Log(Logger.Level.Info, "User confirmed to change to online Sync mode");
                await ManagedDrive?.MainSync?.ChangeMode(Types.SyncType.Online);
            }
            else
            {
                Logger.Log(Logger.Level.Info, "User canceled the change to online Sync mode");
                OnlineRadioButton.IsChecked = false;
                OfflineRadioButton.IsChecked = true;
            }
            OnlineRadioButton.IsEnabled = true;
            OfflineRadioButton.IsEnabled = true;
        }

        private async void LiteSyncRadioButtonUnchecked(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            if (!IsLoaded || !OnlineRadioButton.IsEnabled) return;
            OnlineRadioButton.IsEnabled = false;
            OfflineRadioButton.IsEnabled = false;
            if (await ShowConversionDialog() == ContentDialogResult.Primary)
            {
                Logger.Log(Logger.Level.Info, "User confirmed to change to offline Sync mode");
                await ManagedDrive?.MainSync?.ChangeMode(Types.SyncType.Offline);
            }
            else
            {
                Logger.Log(Logger.Level.Info, "User canceled the change to offline Sync mode");
                OnlineRadioButton.IsChecked = true;
                OfflineRadioButton.IsChecked = false;
            }
            OnlineRadioButton.IsEnabled = true;
            OfflineRadioButton.IsEnabled = true;

        }

        private async Task<ContentDialogResult> ShowConversionDialog()
        {
            ContentDialog dialog = new ContentDialog();

            dialog.XamlRoot = this.XamlRoot;
            dialog.Title = Utility.GetLocalizedString("Page_Settings_DriveManagementPage_SyncMode_WarningDialog/Title");
            dialog.PrimaryButtonText = Utility.GetLocalizedString("Page_Settings_DriveManagementPage_SyncMode_WarningDialog/PrimaryButtonText");
            dialog.SecondaryButtonText = Utility.GetLocalizedString("Page_Settings_DriveManagementPage_SyncMode_WarningDialog/SecondaryButtonText");
            dialog.DefaultButton = ContentDialogButton.Primary;
            dialog.Content = Utility.GetLocalizedString("Page_Settings_DriveManagementPage_SyncMode_WarningDialog/Content");
            var result = await dialog.ShowAsync();
            return result;
        }
    }
}
