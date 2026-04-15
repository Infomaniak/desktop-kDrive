using Infomaniak.kDrive.CustomControls;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.Pages.DriveSetupContentDialog
{
    public sealed partial class SyncExclusionPage : Page
    {
        private readonly AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        public AppModel ViewModel { get { return _viewModel; } }

        private DriveSetupContentDialogVM? DriveSetupContentDialogVM { get; set; }
        private ISync? Sync { get; set; }
        public SyncExclusionPage()
        {
            Logger.Log(Logger.Level.Info, "Navigated to DriveSetupContentDialog.SyncExclusionPage - Initializing DriveSetupContentDialog.SyncExclusionPage components");
            InitializeComponent();
            Logger.Log(Logger.Level.Debug, "DriveSetupContentDialog.SyncExclusionPage components initialized");
        }
        public SyncExclusionPage(ISync sync)
        {
            Logger.Log(Logger.Level.Info, "Navigated to DriveSetupContentDialog.SyncExclusionPage - Initializing DriveSetupContentDialog.SyncExclusionPage components");
            Sync = sync;
            InitializeComponent();
            Logger.Log(Logger.Level.Debug, "DriveSetupContentDialog.SyncExclusionPage components initialized");
        }

        // Navigation method
        protected override async void OnNavigatedTo(NavigationEventArgs e)
        {
            if (e.Parameter is DriveSetupContentDialogVM viewModel)
            {
                DriveSetupContentDialogVM = viewModel;
                Sync = DriveSetupContentDialogVM.CurrentSync;
                DriveSetupContentDialogVM.CurrentStepCancelled += DriveSetupContentDialogVM_CurrentStepCancelled;
                DriveSetupContentDialogVM.CurrentStepConfirmed += DriveSetupContentDialogVM_CurrentStepConfirmed;
                DriveSetupContentDialogVM.SetupFinished += DriveSetupContentDialogVM_SetupFinished;

            }
            else if (Sync is null)
            {
                Logger.Log(Logger.Level.Fatal, "Invalid parameter type when navigating to SyncExclusionPage");
                throw new Exception("Invalid parameter type when navigating to SyncExclusionPage");
            }
        }

        private void DriveSetupContentDialogVM_SetupFinished(object? sender, CustomControls.DriveSetupContentDialog.DriveSetupResult e)
        {
            DetachEventHandlers();
            Bindings.StopTracking();
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

        // Public methods
        public async Task SaveChanges()
        {
            await ExclusionSelector.SaveChanges();
        }

        // Private methods
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
    }
}
