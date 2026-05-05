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
using Infomaniak.kDrive.CustomControls;
using Infomaniak.kDrive.Pages.DriveSetupContentDialog;
using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.Linq;
using System.Threading;

namespace Infomaniak.kDrive.Pages.Settings;

public sealed partial class DriveAdvancedSyncsPage : Page
{
    private readonly AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
    private IDrive? _baseDrive;
    private Drive? ManagedDrive { get; set; }


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

            ManagedDrive = drive;
        }
        else if (_viewModel.AllDrives.Any(d => d.DriveId == _baseDrive.DriveId && d.AccountId == _baseDrive.AccountId && d.UserDbId == _baseDrive.UserDbId))
        {
            // Can happen if a user uses the back button after setting up a new drive.
            Logger.Log(Logger.Level.Info, "The Available drive have an equivalent configured drive that should be used");
            AppModel.UIThreadDispatcher.TryEnqueue(() => { Frame.GoBack(); }); // Frame.GoBack() must be called outside of OnNavigatedTo
            return;
        }
    }


    private void SetupNavBar(string driveName)
    {
        NavBar.ItemsSource = new string[] { Localizer.Instance.GetString("settingsTitle"), driveName, Localizer.Instance.GetString("advancedSyncTitle") };
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

    private async void RemoveSyncButton_Click(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
    {
        var control = sender as Control;
        if (control is null)
        {
            Logger.Log(Logger.Level.Error, "Remove sync button is null when clicking on remove sync button");
            return;
        }

        control.IsEnabled = false;
        Sync? sync = control.DataContext as Sync;
        if (sync is null)
        {
            Logger.Log(Logger.Level.Error, "Could not get sync from DataContext when clicking on remove sync button");
            control.IsEnabled = true;
            return;
        }

        if (ManagedDrive is null)
        {
            Logger.Log(Logger.Level.Error, "Cannot remove sync: ManagedDrive or sync is null");
            control.IsEnabled = true;
            return;
        }

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
            Logger.Log(Logger.Level.Info, "User canceled sync removal");
            control.IsEnabled = true;
            return;
        }

        Logger.Log(Logger.Level.Info, "User confirmed advanced sync removal");
        if (!await sync.Drive.RemoveSync(sync, CancellationToken.None))
        {
            Logger.Log(Logger.Level.Error, "Failed to remove sync");
            Utility.ShowUnexpectedErrorTeachingTip();
        }

        control.IsEnabled = true;
    }

    private async void SyncTypeRadioButton_Click(object sender, RoutedEventArgs e)
    {
        var radioButton = sender as RadioButton;
        if (radioButton is null)
        {
            Logger.Log(Logger.Level.Error, "Sender of SyncTypeRadioButton_Click is not a RadioButton");
            return;
        }

        if (!radioButton.IsEnabled || radioButton.IsChecked != true)
            return;

        Sync? sync = radioButton.DataContext as Sync;
        if (sync is null)
        {
            Logger.Log(Logger.Level.Error, "Could not get sync from DataContext when clicking on sync mode radio button");
            return;
        }

        bool targetOnline = radioButton.Name == "OnlineRadioButton";
        bool targetOffline = radioButton.Name == "OfflineRadioButton";

        if (!targetOffline && !targetOnline)
        {
            Logger.Log(Logger.Level.Error, "Unknown radio button name for sync mode change");
            return;
        }

        if (targetOffline && sync.SyncType == Types.SyncType.Offline)
        {
            Logger.Log(Logger.Level.Info, "User clicked on Offline sync mode radio button while already in Offline mode");
            return;
        }

        if (targetOnline && sync.SyncType == Types.SyncType.Online)
        {
            Logger.Log(Logger.Level.Info, "User clicked on Online sync mode radio button while already in Online mode");
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
            Logger.Log(Logger.Level.Info, "User canceled the change to online Sync mode");
            // This is needed to revert the radio button state back to offline, as changing the sync type to online can fail and we want to reflect that in the UI.
            if (targetOnline)
            {
                sync.SyncType = Types.SyncType.Online;
                sync.SyncType = Types.SyncType.Offline; // Force all the bindings to update, especially the one on the radio buttons.IsChecked
            }
            else
            {
                sync.SyncType = Types.SyncType.Offline;
                sync.SyncType = Types.SyncType.Online; // Force all the bindings to update, especially the one on the radio buttons.IsChecked
            }
            return;
        }


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

    private async void AddAdvancedSyncButton_Click(object sender, RoutedEventArgs e)
    {
        if (_baseDrive is null)
        {
            Logger.Log(Logger.Level.Error, "Cannot add sync: _baseDrive is null");
            return;
        }

        Control? control = sender as Control;
        if (control is not null)
            control.IsEnabled = false;

        CustomControls.AdvancedSyncSetupContentDialog dialog = new(this.XamlRoot, _baseDrive);
        _ = await dialog.ShowAsync();

        if (dialog.Result == CustomControls.AdvancedSyncSetupContentDialog.AdvancedSyncSetupResult.Cancelled)
        {
            Logger.Log(Logger.Level.Info, "User canceled advanced sync creation in content dialog");
            if (control is not null)
                control.IsEnabled = true;
            return;
        }

        var newSync = dialog.GetNewSync();
        if (newSync is null || newSync.Drive is null)
        {
            Logger.Log(Logger.Level.Error, "User confirmed new advanced sync creation in content dialog but GetNewSync returned null or newSync.Drive is null");
            Utility.ShowUnexpectedErrorTeachingTip();
            if (control is not null)
                control.IsEnabled = true;
            return;
        }

        Logger.Log(Logger.Level.Info, "User confirmed new advanced sync creation in content dialog, creating sync");

        var commService = App.ServiceProvider.GetRequiredService<IServerCommService>();
        Logger.Log(Logger.Level.Debug, $"Setting up new sync: LocalPath={newSync.LocalPath}, RemotePath={newSync.RemotePath}, Drive={newSync.Drive.Name}");
        if (!await commService.AddSync(newSync, CancellationToken.None))
        {
            Logger.Log(Logger.Level.Error, $"Failed to add new sync for drive");
            if (control is not null)
                control.IsEnabled = true;
            Utility.ShowUnexpectedErrorTeachingTip();
            return;
        }

        if (control is not null)
            control.IsEnabled = true;
    }

    private async void SyncExclusionButton_Click(object sender, RoutedEventArgs e)
    {
        Control? control = sender as Control;
        if (control is null)
        {
            Logger.Log(Logger.Level.Error, "Sync exclusion button is null when clicking on sync exclusion button");
            Utility.ShowUnexpectedErrorTeachingTip();
            return;
        }

        ISync? sync = control.DataContext as ISync;
        if (sync is null)
        {
            Logger.Log(Logger.Level.Error, "Could not get sync from DataContext when clicking on sync exclusion button");
            Utility.ShowUnexpectedErrorTeachingTip();
            return;
        }

        ContentDialog dialog = new ContentDialog
        {
            XamlRoot = this.XamlRoot,
            CloseButtonText = Localizer.Instance.GetString("buttonCancel"),
            PrimaryButtonText = Localizer.Instance.GetString("buttonConfirm"),
            DefaultButton = ContentDialogButton.Primary
        };
        var exclusionPage = new SyncExclusionPage(sync);
        dialog.Content = exclusionPage;

        var result = await dialog.ShowAsync();
        if (result == ContentDialogResult.Primary)
        {
            Logger.Log(Logger.Level.Info, "User confirmed sync exclusion changes");
            await exclusionPage.SaveChanges();
        }
        else
        {
            Logger.Log(Logger.Level.Info, "User canceled sync exclusion changes");
        }

    }
}
