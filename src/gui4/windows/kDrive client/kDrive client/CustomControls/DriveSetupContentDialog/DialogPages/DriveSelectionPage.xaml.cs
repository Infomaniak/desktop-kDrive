using Infomaniak.kDrive.CustomControls;
using Infomaniak.kDrive.OnBoarding;
using Infomaniak.kDrive.Pages.Onboarding;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.Linq;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.Pages.DriveSetupContentDialog
{
    public sealed partial class DriveSelectionPage : Page
    {
        private AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        public AppModel ViewModel { get { return _viewModel; } }

        private DriveSetupContentDialogVM? DriveSetupContentDialogVM { get; set; }

        public DriveSelectionPage()
        {
            Logger.Log(Logger.Level.Info, "Navigated to DriveSetupContentDialog.DriveSelectionPage - Initializing DriveSetupContentDialog.DriveSelectionPage components");
            InitializeComponent();
            Logger.Log(Logger.Level.Debug, "DriveSetupContentDialog.DriveSelectionPage components initialized");
        }

        // Navigation method
        protected override async void OnNavigatedTo(NavigationEventArgs e)
        {
            if (e.Parameter is DriveSetupContentDialogVM viewModel)
            {
                DriveSetupContentDialogVM = viewModel;

                if (!DriveSetupContentDialogVM.NewSyncs.Any())
                {
                    Logger.Log(Logger.Level.Fatal, "No NewSyncs found in DriveSetupContentDialogVM when navigating to DriveSelectionPage");
                    DriveSetupContentDialogVM.FinishSetup();
                    return;
                }

                if (!DriveSetupContentDialogVM.IsMultipleDrivesSetup())
                {
                    GoToSyncSetupPagePage(DriveSetupContentDialogVM.NewSyncs[0]);
                    return;
                }

                DriveSetupContentDialogVM.CurrentStepConfirmed += DriveSetupContentDialogVM_CurrentStepConfirmed;
                DriveSetupContentDialogVM.CurrentStepCancelled += DriveSetupContentDialogVM_CurrentStepCancelled;
            }
            else
            {
                Logger.Log(Logger.Level.Fatal, "DriveSetupContentDialogVM parameter missing when navigating to DriveSelectionPage");
                throw new Exception("DriveSetupContentDialogVM parameter missing when navigating to DriveSelectionPage");
            }
        }

        protected override void OnNavigatedFrom(NavigationEventArgs e)
        {
            if (DriveSetupContentDialogVM is not null)
            {
                DriveSetupContentDialogVM.CurrentStepConfirmed -= DriveSetupContentDialogVM_CurrentStepConfirmed;
                DriveSetupContentDialogVM.CurrentStepCancelled -= DriveSetupContentDialogVM_CurrentStepCancelled;
            }
        }

        private void DriveSetupContentDialogVM_CurrentStepCancelled(object? sender, EventArgs e)
        {
            DriveSetupContentDialogVM?.FinishSetup();
        }

        private void DriveSetupContentDialogVM_CurrentStepConfirmed(object? sender, EventArgs e)
        {
            DriveSetupContentDialogVM?.FinishSetup();
        }

        private void EditButton_Click(object sender, RoutedEventArgs e)
        {
            if (sender is Control control && control.DataContext is NewSync sync)
            {
                GoToSyncSetupPagePage(sync);
            }
            else
            {
                Logger.Log(Logger.Level.Error, "Unable to retrieve NewSync from DataContext");
            }
        }

        private void GoToSyncSetupPagePage(NewSync sync)
        {
            if (DriveSetupContentDialogVM is null)
            {
                Logger.Log(Logger.Level.Error, "DriveSetupContentDialogVM is null");
                return;
            }

            DriveSetupContentDialogVM.CurrentSync = sync;
            AppModel.UIThreadDispatcher.TryEnqueue(() => Frame.Navigate(typeof(SyncSetupPage), DriveSetupContentDialogVM));
        }
    }
}
