using Infomaniak.kDrive.CustomControls;
using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.ComponentModel;
using System.Threading;
using Windows.Storage.Pickers;

namespace Infomaniak.kDrive.Pages.AdvancedSyncSetupContentDialog
{
    public sealed partial class SyncSetupPage : Page
    {
        private readonly AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        public AppModel ViewModel { get { return _viewModel; } }

        private AdvancedSyncSetupContentDialogVM? AdvancedSyncSetupContentDialogVM { get; set; }
        public SyncSetupPage()
        {
            Logger.Log(Logger.Level.Info, "Navigated to AdvancedSyncSetupContentDialog.SyncSetupPage - Initializing AdvancedSyncSetupContentDialog.SyncSetupPage components");
            InitializeComponent();
            Logger.Log(Logger.Level.Debug, "AdvancedSyncSetupContentDialog.SyncSetupPage components initialized");
        }

        // Navigation method
        protected override void OnNavigatedTo(NavigationEventArgs e)
        {
            if (e.Parameter is AdvancedSyncSetupContentDialogVM viewModel)
            {
                AdvancedSyncSetupContentDialogVM = viewModel;
                AdvancedSyncSetupContentDialogVM.CurrentStepCancelled += AdvancedSyncSetupContentDialogVM_CurrentStepCancelled;
                AdvancedSyncSetupContentDialogVM.CurrentStepConfirmed += AdvancedSyncSetupContentDialogVM_CurrentStepConfirmed;
                AdvancedSyncSetupContentDialogVM.NewSync.PropertyChanged += AdvancedSyncSetupContentDialogVMNewSync_PropertyChanged;
                UpdateCanGoNext();
            }
            else
            {
                Logger.Log(Logger.Level.Fatal, "Invalid parameter type when navigating to SyncSetupPage");
                throw new Exception("Invalid parameter type when navigating to SyncSetupPage");
            }
        }

        protected override void OnNavigatedFrom(NavigationEventArgs e)
        {
            if (AdvancedSyncSetupContentDialogVM is null)
            {
                Logger.Log(Logger.Level.Error, "AdvancedSyncSetupContentDialogVM is null");
                return;
            }

            AdvancedSyncSetupContentDialogVM.CurrentStepCancelled -= AdvancedSyncSetupContentDialogVM_CurrentStepCancelled;
            AdvancedSyncSetupContentDialogVM.CurrentStepConfirmed -= AdvancedSyncSetupContentDialogVM_CurrentStepConfirmed;
            AdvancedSyncSetupContentDialogVM.NewSync.PropertyChanged -= AdvancedSyncSetupContentDialogVMNewSync_PropertyChanged;
        }

        private void AdvancedSyncSetupContentDialogVMNewSync_PropertyChanged(object? sender, PropertyChangedEventArgs e)
        {
            if (e.PropertyName == nameof(NewSync.LocalPath) || e.PropertyName == nameof(NewSync.RemotePath) || e.PropertyName == nameof(NewSync.RemoteNodeId))
                UpdateCanGoNext();
        }

        private void UpdateCanGoNext()
        {
            if (AdvancedSyncSetupContentDialogVM is null)
            {
                Logger.Log(Logger.Level.Error, "AdvancedSyncSetupContentDialogVM?.NewSync is null");
                return;
            }

            AdvancedSyncSetupContentDialogVM.CanGoNext =
                    !string.IsNullOrEmpty(AdvancedSyncSetupContentDialogVM.NewSync.LocalPath) &&
                    !string.IsNullOrEmpty(AdvancedSyncSetupContentDialogVM.NewSync.RemotePath) &&
                    !string.IsNullOrEmpty(AdvancedSyncSetupContentDialogVM.NewSync.RemoteNodeId);
        }

        private void AdvancedSyncSetupContentDialogVM_CurrentStepConfirmed(object? sender, EventArgs e)
        {
            if (AdvancedSyncSetupContentDialogVM is null)
            {
                Logger.Log(Logger.Level.Error, "AdvancedSyncSetupContentDialogVM is null");
                return;
            }

            AdvancedSyncSetupContentDialogVM.FinishSetup(CustomControls.AdvancedSyncSetupContentDialog.AdvancedSyncSetupResult.Confirmed);
        }

        private async void AdvancedSyncSetupContentDialogVM_CurrentStepCancelled(object? sender, EventArgs e)
        {
            if (AdvancedSyncSetupContentDialogVM is null)
            {
                Logger.Log(Logger.Level.Error, "AdvancedSyncSetupContentDialogVM is null");
                return;
            }

            AdvancedSyncSetupContentDialogVM.FinishSetup(CustomControls.AdvancedSyncSetupContentDialog.AdvancedSyncSetupResult.Cancelled);
        }

        private async void SelectLocalFolder_Click(object sender, RoutedEventArgs e)
        {
            Logger.Log(Logger.Level.Info, "Change sync path button clicked, opening folder picker");

            if (AdvancedSyncSetupContentDialogVM?.NewSync is null)
            {
                Logger.Log(Logger.Level.Error, "AdvancedSyncSetupContentDialogVM?.NewSync is null");
                return;
            }
            var newSync = AdvancedSyncSetupContentDialogVM.NewSync;

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

            var commServices = App.ServiceProvider.GetRequiredService<IServerCommService>();
            bool? result = await commServices.IsPathValidForNewSync(folder.Path, SyncConfiguration.Advanced, CancellationToken.None);
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

        private void RemoteLocationButton_Click(object sender, RoutedEventArgs e)
        {
            Frame.Navigate(typeof(RemoteLocationSelectionPage), AdvancedSyncSetupContentDialogVM);
        }

        private Visibility IsStringEmptyToVisibility(string str, bool invert = false)
        {
            bool isEmpty = string.IsNullOrEmpty(str);
            if (invert)
                isEmpty = !isEmpty;
            return isEmpty ? Visibility.Visible : Visibility.Collapsed;
        }
    }
}
