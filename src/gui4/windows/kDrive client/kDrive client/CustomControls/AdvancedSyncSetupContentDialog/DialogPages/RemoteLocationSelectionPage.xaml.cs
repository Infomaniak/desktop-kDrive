using Infomaniak.kDrive.CustomControls;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using System;

namespace Infomaniak.kDrive.Pages.AdvancedSyncSetupContentDialog
{
    public sealed partial class RemoteLocationSelectionPage : Page
    {
        private readonly AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        public AppModel ViewModel { get { return _viewModel; } }

        private AdvancedSyncSetupContentDialogVM? AdvancedSyncSetupContentDialogVM { get; set; }
        public RemoteLocationSelectionPage()
        {
            Logger.Log(Logger.Level.Info, "Navigated to DriveSetupContentDialog.RemoteLocationSelectionPage - Initializing DriveSetupContentDialog.RemoteLocationSelectionPage components");
            InitializeComponent();
            Logger.Log(Logger.Level.Debug, "DriveSetupContentDialog.RemoteLocationSelectionPage components initialized");
        }

        // Navigation method
        protected override async void OnNavigatedTo(NavigationEventArgs e)
        {
            if (e.Parameter is AdvancedSyncSetupContentDialogVM viewModel)
            {
                AdvancedSyncSetupContentDialogVM = viewModel;
                AdvancedSyncSetupContentDialogVM.CurrentStepCancelled += AdvancedSyncSetupContentDialogVM_CurrentStepCancelled;
                AdvancedSyncSetupContentDialogVM.CurrentStepConfirmed += AdvancedSyncSetupContentDialogVM_CurrentStepConfirmed;
            }
            else
            {
                Logger.Log(Logger.Level.Fatal, "Invalid parameter type when navigating to RemoteLocationSelectionPage");
                throw new Exception("Invalid parameter type when navigating to RemoteLocationSelectionPage");
            }
        }
        protected override void OnNavigatedFrom(NavigationEventArgs e)
        {
            AdvancedSyncSetupContentDialogVM!.CurrentStepCancelled -= AdvancedSyncSetupContentDialogVM_CurrentStepCancelled;
            AdvancedSyncSetupContentDialogVM!.CurrentStepConfirmed -= AdvancedSyncSetupContentDialogVM_CurrentStepConfirmed;
        }

        private async void AdvancedSyncSetupContentDialogVM_CurrentStepConfirmed(object? sender, EventArgs e)
        {
            if (AdvancedSyncSetupContentDialogVM is null)
            {
                Logger.Log(Logger.Level.Error, "AdvancedSyncSetupContentDialogVM is null");
                return;
            }
          //  await ExclusionSelector.SaveChanges();

            Frame.Navigate(typeof(SyncSetupPage), AdvancedSyncSetupContentDialogVM);
        }

        private void AdvancedSyncSetupContentDialogVM_CurrentStepCancelled(object? sender, EventArgs e)
        {
            if (AdvancedSyncSetupContentDialogVM is null)
            {
                Logger.Log(Logger.Level.Error, "AdvancedSyncSetupContentDialogVM is null");
                return;
            }

            Frame.Navigate(typeof(SyncSetupPage), AdvancedSyncSetupContentDialogVM);
        }
    }
}
