using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using System;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Infomaniak.kDrive.Pages.Settings;

public sealed partial class DriveAdvancedSyncsPage : Page
{
    private AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
    private IDrive? _baseDrive;
    private Drive? _managedDrive { get; set; }


    public DriveAdvancedSyncsPage()
    {
        InitializeComponent();
        SetupNavBar("");
    }

    protected override void OnNavigatedTo(NavigationEventArgs e)
    {
        _baseDrive = e.Parameter as IDrive;
        if (_baseDrive is null)
        {
            var errorMessage = "Drive parameter missing when navigating to DriveManagementPage";
            Logger.Log(Logger.Level.Error, errorMessage);
            AppModel.UIThreadDispatcher.TryEnqueue(() => { Frame.GoBack(); }); // Frame.GoBack() must be called outside of OnNavigatedTo
            return;
        }
        SetupNavBar(_baseDrive.Name);

        if (_baseDrive is ViewModels.Drive drive)
        {
            if (!_viewModel.AllDrives.Contains(drive))
            {
                // Can happen if a user uses the back button after being redirected to the settings page following a drive deletion.
                AppModel.UIThreadDispatcher.TryEnqueue(() => { Frame.GoBack(); }); // Frame.GoBack() must be called outside of OnNavigatedTo
                return;
            }

            _managedDrive = drive;
        }
    }


    private void SetupNavBar(string driveName)
    {
        NavBar.ItemsSource = new string[] { Utility.GetLocalizedString("Page_SettingsPage_Title/Text"), Utility.GetLocalizedString("Page_DriveManagement_Title/Text"), driveName, Utility.GetLocalizedString("Page_DriveAdvancedSyncsPage_Title/Text") };
    }

    private void NavBar_ItemClicked(BreadcrumbBar sender, BreadcrumbBarItemClickedEventArgs args)
    {
        if (args.Index == 0)
        {
            Logger.Log(Logger.Level.Debug, "Navigating to SettingsPage");
            Frame.Navigate(typeof(SettingsPage));
        }
        else if (args.Index >= 1 && args.Index <= 2)
        {
            Logger.Log(Logger.Level.Debug, "Navigating to DriveManagementPage");
            Frame.Navigate(typeof(DriveManagementPage), _baseDrive);
        }
    }
    private async void LocalPathLink_Click(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
    {
        Sync? sync = (sender as FrameworkElement)?.DataContext as Sync;
        if (sync is null)
        {
            Logger.Log(Logger.Level.Error, "Could not get sync from DataContext when clicking on local path link");
            return;
        }

        await Utility.OpenFolderSecurely(sync.LocalPath);
    }

    private async void RemotePathLink_Click(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
    {
        Sync? sync = (sender as FrameworkElement)?.DataContext as Sync;
        if (sync is null)
        {
            Logger.Log(Logger.Level.Error, "Could not get sync from DataContext when clicking on remote path link");
            return;
        }

        Uri remoteFolderWebUri = App.Constants.Drive.itemUri(sync.Drive.DriveId, sync.RemoteNodeId);
        await Windows.System.Launcher.LaunchUriAsync(remoteFolderWebUri);
    }

    private async void OnlineRadioButtonChecked(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
    {
        var radioButton = sender as RadioButton;
        if (!IsLoaded || radioButton is null || !radioButton.IsEnabled)
            return;

        Logger.Log(Logger.Level.Info, "User initiated change to online Sync mode");
        bool succeed = false;
        bool canceledByUser = await Utility.ShowContentDialogAsync(this.XamlRoot, "Page_Settings_DriveManagementPage_SyncMode_WarningDialog") == ContentDialogResult.Primary;

        var revertFunc = () =>
        {
            bool prevState = radioButton.IsEnabled;
            radioButton.IsEnabled = false;
            radioButton.IsChecked = false;
            radioButton.IsEnabled = prevState;
        };

        if (canceledByUser)
        {
            Logger.Log(Logger.Level.Info, "User canceled the change to online Sync mode");
            revertFunc();
            return;
        }

        Logger.Log(Logger.Level.Info, "User confirmed to change to online Sync mode");
        Sync? sync = radioButton.DataContext as Sync;
        if (sync is not null)
        {
            succeed = await sync.ChangeSyncType(Types.SyncType.Online);
        }
        else
        {
            Logger.Log(Logger.Level.Error, "Cannot change sync mode: radioButton.DataContext is null or not a sync");
            succeed = false;
        }


        if (!succeed)
        {
            await Utility.ShowContentDialogAsync(this.XamlRoot, "Page_Settings_DriveManagementPage_SyncMode_ErrorDialog");
            revertFunc();
        }
    }

    private async void OnlineRadioButtonUnchecked(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
    {
        var radioButton = sender as RadioButton;
        if (!IsLoaded || radioButton is null || !radioButton.IsEnabled)
            return;

        Logger.Log(Logger.Level.Info, "User initiated change to offline Sync mode");
        bool succeed = false;
        bool canceledByUser = await Utility.ShowContentDialogAsync(this.XamlRoot, "Page_Settings_DriveManagementPage_SyncMode_WarningDialog") == ContentDialogResult.Primary;

        var revertFunc = () =>
        {
            bool prevState = radioButton.IsEnabled;
            radioButton.IsEnabled = false;
            radioButton.IsChecked = true;
            radioButton.IsEnabled = prevState;
        };

        if (canceledByUser)
        {
            Logger.Log(Logger.Level.Info, "User canceled the change to online Sync mode");
            revertFunc();
            return;
        }

        Logger.Log(Logger.Level.Info, "User confirmed to change to offline Sync mode");
        Sync? sync = radioButton.DataContext as Sync;
        if (sync is not null)
        {
            succeed = await sync.ChangeSyncType(Types.SyncType.Offline);
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
}
