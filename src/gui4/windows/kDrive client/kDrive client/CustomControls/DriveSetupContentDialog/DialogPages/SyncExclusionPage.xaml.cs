using Infomaniak.kDrive.CustomControls;
using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using Windows.Storage.Pickers;
using System;
using System.Threading;
using System.Linq;

namespace Infomaniak.kDrive.Pages.DriveSetupContentDialog
{
    public sealed partial class SyncExclusionPage : Page
    {
        private AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        public AppModel ViewModel { get { return _viewModel; } }

        private DriveSetupContentDialogVM? DriveSetupContentDialogVM { get; set; }
        public SyncExclusionPage()
        {
            Logger.Log(Logger.Level.Info, "Navigated to DriveSetupContentDialog.SyncExclusionPage - Initializing DriveSetupContentDialog.SyncExclusionPage components");
            InitializeComponent();
            Logger.Log(Logger.Level.Debug, "DriveSetupContentDialog.SyncExclusionPage components initialized");
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
                Logger.Log(Logger.Level.Fatal, "Invalid parameter type when navigating to SyncExclusionPage");
                throw new Exception("Invalid parameter type when navigating to SyncSetupPage");
            }
        }
        protected override void OnNavigatedFrom(NavigationEventArgs e)
        {
            DriveSetupContentDialogVM!.CurrentStepCancelled -= DriveSetupContentDialogVM_CurrentStepCancelled;
            DriveSetupContentDialogVM!.CurrentStepConfirmed -= DriveSetupContentDialogVM_CurrentStepConfirmed;
        }

        private async void DriveSetupContentDialogVM_CurrentStepConfirmed(object? sender, EventArgs e)
        {
            if (DriveSetupContentDialogVM is null)
            {
                Logger.Log(Logger.Level.Error, "DriveSetupContentDialogVM is null");
                return;
            }
            await ExclusionSelector.SaveChanges();

            Frame.Navigate(typeof(SyncSetupPage), DriveSetupContentDialogVM);
        }

        private void DriveSetupContentDialogVM_CurrentStepCancelled(object? sender, EventArgs e)
        {
            if (DriveSetupContentDialogVM is null)
            {
                Logger.Log(Logger.Level.Error, "DriveSetupContentDialogVM is null");
                return;
            }

            Frame.Navigate(typeof(SyncSetupPage), DriveSetupContentDialogVM);
        }

        private async void ChangeFolder_Click(object sender, RoutedEventArgs e)
        {
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

            if (DriveSetupContentDialogVM.NewSyncs.Any(s => s.LocalPath.Equals(folder.Path, StringComparison.OrdinalIgnoreCase)))
            {
                Logger.Log(Logger.Level.Warning, $"Selected folder path '{folder.Path}' is already used by another sync.");
                control.IsEnabled = true;
                return;
            }

            var commServices = App.ServiceProvider.GetRequiredService<IServerCommService>();
            bool? result = await commServices.IsPathValidForNewSync(folder.Path, CancellationToken.None);
            if (result is null)
            {
                Utility.ShowUnexpectedErrorTeachingTip();
                Logger.Log(Logger.Level.Error, $"Unable to validate selected folder path '{folder.Path}' for syncing due to an unexpected error");
                control.IsEnabled = true;
                return;
            }
            if (!result.Value)
            {
                Utility.ShowTeachingTipFromxUid("CC_DriveSetupContentDialog_SyncSetupPage_TeachingTip_InvalidFolder");
                Logger.Log(Logger.Level.Info, $"Selected folder path '{folder.Path}' is not valid for syncing");
                control.IsEnabled = true;
                return;
            }

            newSync.LocalPath = folder.Path;
            await newSync.SelectBestVfsMode();
            Logger.Log(Logger.Level.Info, $"Sync path for drive '{newSync.Drive?.Name ?? "unknown"}' updated to '{newSync.LocalPath}' with sync type '{newSync.SyncType}'");
            control.IsEnabled = true;
        }
    }
}
