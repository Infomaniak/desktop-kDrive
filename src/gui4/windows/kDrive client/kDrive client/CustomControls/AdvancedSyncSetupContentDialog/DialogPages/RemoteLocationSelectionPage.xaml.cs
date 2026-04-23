using Infomaniak.kDrive.CustomControls;
using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.Threading;

namespace Infomaniak.kDrive.Pages.AdvancedSyncSetupContentDialog
{
    public sealed partial class RemoteLocationSelectionPage : Page
    {
        private readonly AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        private long RemoteLocationSelectorHasSelectedNodePropertyToken;
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
                AdvancedSyncSetupContentDialogVM.CanGoNext = false;
                RemoteLocationSelector.Loaded += RemoteLocationSelector_Loaded;
            }
            else
            {
                Logger.Log(Logger.Level.Fatal, "Invalid parameter type when navigating to RemoteLocationSelectionPage");
                throw new Exception("Invalid parameter type when navigating to RemoteLocationSelectionPage");
            }
        }

        protected override void OnNavigatedFrom(NavigationEventArgs e)
        {
            DetachEventHandlers();
        }


        private void DetachEventHandlers()
        {
            RemoteLocationSelector.Loaded -= RemoteLocationSelector_Loaded;

            if (AdvancedSyncSetupContentDialogVM is null)
            {
                Logger.Log(Logger.Level.Error, "AdvancedSyncSetupContentDialogVM is null");
                return;
            }

            AdvancedSyncSetupContentDialogVM.CurrentStepCancelled -= AdvancedSyncSetupContentDialogVM_CurrentStepCancelled;
            AdvancedSyncSetupContentDialogVM.CurrentStepConfirmed -= AdvancedSyncSetupContentDialogVM_CurrentStepConfirmed;
            RemoteLocationSelector.UnregisterPropertyChangedCallback(RemoteLocationSelector.HasSelectedNodeProperty, RemoteLocationSelectorHasSelectedNodePropertyToken);
        }

        private void RemoteLocationSelector_Loaded(object sender, RoutedEventArgs e)
        {
            RemoteLocationSelectorHasSelectedNodePropertyToken = RemoteLocationSelector.RegisterPropertyChangedCallback(RemoteLocationSelector.HasSelectedNodeProperty, (_, _) => UpdateCanGoNext());
        }

        private void UpdateCanGoNext()
        {
            if (AdvancedSyncSetupContentDialogVM is null)
            {
                Logger.Log(Logger.Level.Error, "AdvancedSyncSetupContentDialogVM is null");
                return;
            }

            AdvancedSyncSetupContentDialogVM.CanGoNext = RemoteLocationSelector.HasSelectedNode;
        }


        private async void AdvancedSyncSetupContentDialogVM_CurrentStepConfirmed(object? sender, EventArgs e)
        {
            if (AdvancedSyncSetupContentDialogVM is null)
            {
                Logger.Log(Logger.Level.Error, "AdvancedSyncSetupContentDialogVM is null");
                return;
            }

            var selectedNodeId = RemoteLocationSelector.GetSelectedNodeId();
            if (selectedNodeId is null)
            {
                Logger.Log(Logger.Level.Error, "Selected node is null when confirming remote location selection");
                return;
            }

            if (AdvancedSyncSetupContentDialogVM.NewSync is null)
            {
                Logger.Log(Logger.Level.Error, "NewSync is null in AdvancedSyncSetupContentDialogVM when confirming remote location selection");
                return;
            }

            var commService = App.ServiceProvider.GetRequiredService<IServerCommService>();
            IDrive? drive = AdvancedSyncSetupContentDialogVM.NewSync.Drive;
            if (drive is null)
            {
                Logger.Log(Logger.Level.Error, "Drive is null in NewSync when confirming remote location selection");
                return;
            }

            var getNodeInfoResult = await commService.GetNodeInfo(drive.UserDbId, drive.DriveId, selectedNodeId, CancellationToken.None);
            if (!getNodeInfoResult.IsSuccess || getNodeInfoResult.Node is null)
            {
                Logger.Log(Logger.Level.Error, $"Failed to get node info for node ID {selectedNodeId} when confirming remote location selection");
                Utility.ShowUnexpectedErrorTeachingTip();
                return;
            }

            AdvancedSyncSetupContentDialogVM.NewSync.RemotePath = getNodeInfoResult.Node.Path;
            AdvancedSyncSetupContentDialogVM.NewSync.RemoteNodeId = selectedNodeId;
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
