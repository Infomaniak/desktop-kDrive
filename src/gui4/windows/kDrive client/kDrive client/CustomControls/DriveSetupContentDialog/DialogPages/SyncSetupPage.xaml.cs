using Infomaniak.kDrive.CustomControls;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using System;

namespace Infomaniak.kDrive.Pages.DriveSetupContentDialog
{
    public sealed partial class SyncSetupPage : Page
    {
        private AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
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

        private void DriveSetupContentDialogVM_CurrentStepConfirmed(object? sender, EventArgs e)
        {
            if (DriveSetupContentDialogVM!.IsMultipleDrivesSetup())
            {
                Frame.Navigate(typeof(DriveSelectionPage), DriveSetupContentDialogVM);
            }
            else
            {
                DriveSetupContentDialogVM.FinishSetup();
            }
        }

        private void DriveSetupContentDialogVM_CurrentStepCancelled(object? sender, EventArgs e)
        {
            if (DriveSetupContentDialogVM!.IsMultipleDrivesSetup())
            {
                Frame.Navigate(typeof(DriveSelectionPage), DriveSetupContentDialogVM);
            }
            else
            {
                DriveSetupContentDialogVM.FinishSetup();
            }
        }

        protected override void OnNavigatedFrom(NavigationEventArgs e)
        {
        }

        private void EditButton_Click(object sender, RoutedEventArgs e)
        {
            if (sender is Control control && control.DataContext is NewSync sync)
            {
                GoToSyncConfigurationPage(sync);
            }
            else
            {
                Logger.Log(Logger.Level.Error, "Unable to retrieve NewSync from DataContext");
            }
        }

        private void GoToSyncConfigurationPage(NewSync sync)
        {
        }
    }
}
