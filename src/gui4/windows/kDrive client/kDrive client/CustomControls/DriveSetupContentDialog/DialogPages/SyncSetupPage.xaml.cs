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
using Infomaniak.kDrive.CustomControls;
using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.IO;
using System.Linq;
using System.Threading;
using Windows.Storage.Pickers;

namespace Infomaniak.kDrive.Pages.DriveSetupContentDialog
{
    public sealed partial class SyncSetupPage : Page
    {
        private readonly IAnalyticsService _analyticsService = App.ServiceProvider.GetRequiredService<IAnalyticsService>();
        private readonly AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        public AppModel ViewModel { get { return _viewModel; } }

        private DriveSetupContentDialogVM? DriveSetupContentDialogVM { get; set; }
        public SyncSetupPage()
        {
            Logger.Log(Logger.Level.Info, "Navigated to DriveSetupContentDialog.SyncSetupPage - Initializing DriveSetupContentDialog.SyncSetupPage components");
            InitializeComponent();
            Logger.Log(Logger.Level.Debug, "DriveSetupContentDialog.SyncSetupPage components initialized");
        }

        // Navigation method
        protected override async void OnNavigatedTo(NavigationEventArgs e)
        {
            if (e.Parameter is DriveSetupContentDialogVM viewModel)
            {
                DriveSetupContentDialogVM = viewModel;
                DriveSetupContentDialogVM.CurrentStepCancelled += DriveSetupContentDialogVM_CurrentStepCancelled;
                DriveSetupContentDialogVM.CurrentStepConfirmed += DriveSetupContentDialogVM_CurrentStepConfirmed;
            }
            else
            {
                Logger.Log(Logger.Level.Fatal, "Invalid parameter type when navigating to SyncSetupPage");
                throw new Exception("Invalid parameter type when navigating to SyncSetupPage");
            }
        }

        protected override void OnNavigatedFrom(NavigationEventArgs e)
        {
            DetachEventHandlers();
        }

        private void DetachEventHandlers()
        {
            if (DriveSetupContentDialogVM is not null)
            {
                DriveSetupContentDialogVM.CurrentStepCancelled -= DriveSetupContentDialogVM_CurrentStepCancelled;
                DriveSetupContentDialogVM.CurrentStepConfirmed -= DriveSetupContentDialogVM_CurrentStepConfirmed;
            }
        }

        private void DriveSetupContentDialogVM_CurrentStepConfirmed(object? sender, EventArgs e)
        {
            if (DriveSetupContentDialogVM is null)
            {
                Logger.Log(Logger.Level.Error, "DriveSetupContentDialogVM is null");
                return;
            }

            if (DriveSetupContentDialogVM.IsMultipleDrivesSetup())
            {
                Frame.Navigate(typeof(DriveSelectionPage), DriveSetupContentDialogVM);
            }
            else
            {
                DriveSetupContentDialogVM.FinishSetup(CustomControls.DriveSetupContentDialog.DriveSetupResult.Confirmed);
            }
        }

        private async void DriveSetupContentDialogVM_CurrentStepCancelled(object? sender, EventArgs e)
        {
            if (DriveSetupContentDialogVM is null)
            {
                Logger.Log(Logger.Level.Error, "DriveSetupContentDialogVM is null");
                return;
            }
            await DriveSetupContentDialogVM.RevertCurrentSyncChanges();

            if (DriveSetupContentDialogVM.IsMultipleDrivesSetup())
            {
                Frame.Navigate(typeof(DriveSelectionPage), DriveSetupContentDialogVM);
            }
            else
            {
                DriveSetupContentDialogVM.FinishSetup(CustomControls.DriveSetupContentDialog.DriveSetupResult.Cancelled);
            }
        }

        private async void ChangeFolder_Click(object sender, RoutedEventArgs e)
        {
            _analyticsService.TrackClick(Analytics.Keys.Category.DriveSetupDialog, Analytics.Keys.EventName.ChangeSyncLocalLocation);
            Logger.Log(Logger.Level.Info, "Change sync path button clicked, opening folder picker");

            if (DriveSetupContentDialogVM?.CurrentSync is null)
            {
                Logger.Log(Logger.Level.Error, "DriveSetupContentDialogVM?.CurrentSync is null");
                return;
            }
            var newSync = DriveSetupContentDialogVM.CurrentSync;

            //disable the button to avoid double-clicking
            var control = sender as Control;
            if (control is null)
            {
                Logger.Log(Logger.Level.Error, "Sender is not a Control");
                return;
            }

            control.IsEnabled = false;

            // Create a folder picker
            FolderPicker openPicker = new();
            var window = ((App)Application.Current)?.CurrentWindow;
            var hWnd = WinRT.Interop.WindowNative.GetWindowHandle(window);
            WinRT.Interop.InitializeWithWindow.Initialize(openPicker, hWnd);
            openPicker.SuggestedStartLocation = PickerLocationId.ComputerFolder;
            Windows.Storage.StorageFolder folder = await openPicker.PickSingleFolderAsync();

            if (folder is null)
            {
                Logger.Log(Logger.Level.Info, "No folder was picked");
                control.IsEnabled = true;
                return;
            }

            Logger.Log(Logger.Level.Info, "Folder picked: " + folder.Path);

            if (DriveSetupContentDialogVM.NewSyncs.Any(s => s != newSync && s.LocalPath.Equals(folder.Path, StringComparison.OrdinalIgnoreCase)))
            {
                Logger.Log(Logger.Level.Info, $"Selected folder path '{folder.Path}' is already used by another sync.");
                Utility.ShowTeachingTip(Localizer.Instance.GetString("teachingTipInvalidFolderTitle"), Localizer.Instance.GetString("teachingTipInvalidFolderContent"), TimeSpan.FromSeconds(20));

                control.IsEnabled = true;
                return;
            }

            if (DriveSetupContentDialogVM.NewSyncs.Any(s => s != newSync && IsSubPathOf(s.LocalPath, folder.Path)))
            {
                Logger.Log(Logger.Level.Info, $"Selected folder path '{folder.Path}' is already the parent of another sync.");
                Utility.ShowTeachingTip(Localizer.Instance.GetString("teachingTipInvalidFolderTitle"), Localizer.Instance.GetString("teachingTipInvalidFolderContent"), TimeSpan.FromSeconds(20));

                control.IsEnabled = true;
                return;
            }

            if (DriveSetupContentDialogVM.NewSyncs.Any(s => s != newSync && IsSubPathOf(folder.Path, s.LocalPath)))
            {
                Logger.Log(Logger.Level.Info, $"Selected folder path '{folder.Path}' is inside another sync.");
                Utility.ShowTeachingTip(Localizer.Instance.GetString("teachingTipInvalidFolderTitle"), Localizer.Instance.GetString("teachingTipInvalidFolderContent"), TimeSpan.FromSeconds(20));

                control.IsEnabled = true;
                return;
            }

            var commServices = App.ServiceProvider.GetRequiredService<IServerCommService>();
            bool? result = await commServices.IsPathValidForNewSync(folder.Path, SyncConfiguration.Classic, CancellationToken.None);
            if (result is null)
            {
                Utility.ShowUnexpectedErrorTeachingTip();
                Logger.Log(Logger.Level.Error, $"Unable to validate selected folder path '{folder.Path}' for syncing due to an unexpected error");
                control.IsEnabled = true;
                return;
            }
            if (!result.Value)
            {
                Utility.ShowTeachingTip(Localizer.Instance.GetString("teachingTipInvalidFolderTitle"), Localizer.Instance.GetString("teachingTipInvalidFolderContent"), TimeSpan.FromSeconds(20));
                Logger.Log(Logger.Level.Info, $"Selected folder path '{folder.Path}' is not valid for syncing");
                control.IsEnabled = true;
                return;
            }

            newSync.LocalPath = folder.Path;
            await newSync.SelectBestVfsMode();
            Logger.Log(Logger.Level.Info, $"Sync path for drive '{newSync.Drive?.Name ?? "unknown"}' updated to '{newSync.LocalPath}' with sync type '{newSync.SyncType}'");
            control.IsEnabled = true;
        }

        private static bool IsSubPathOf(string candidatePath, string parentPath)
        {
            string normalizedCandidate = Path.GetFullPath(candidatePath)
                .TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);

            string normalizedParent = Path.GetFullPath(parentPath)
                .TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);

            return normalizedCandidate.StartsWith(
                       normalizedParent + Path.DirectorySeparatorChar,
                       StringComparison.OrdinalIgnoreCase)
                   || string.Equals(
                       normalizedCandidate,
                       normalizedParent,
                       StringComparison.OrdinalIgnoreCase);
        }

        private void Exclusionbutton_click(object sender, RoutedEventArgs e)
        {
            _analyticsService.TrackClick(Analytics.Keys.Category.DriveSetupDialog, Analytics.Keys.EventName.ChangeSyncExclusions);
            Frame.Navigate(typeof(SyncExclusionPage), DriveSetupContentDialogVM);
        }

        private async void ReturnToDefaultFolderButton_Click(object sender, RoutedEventArgs e)
        {
            if (DriveSetupContentDialogVM?.CurrentSync is null)
            {
                Utility.ShowUnexpectedErrorTeachingTip();
                return;
            }

            var newSync = DriveSetupContentDialogVM.CurrentSync;

            newSync.LocalPath = newSync.DefaultPath;
            await newSync.SelectBestVfsMode();
        }

        private void LiteSyncToggle_Toggled(object sender, RoutedEventArgs e)
        {
            if (IsLoaded && sender is ToggleSwitch toggle)
                _analyticsService.TrackClick(Analytics.Keys.Category.DriveSetupDialog, Analytics.Keys.EventName.ChangeSyncMode, toggle.IsOn ? 1 : 0);
        }
    }
}
