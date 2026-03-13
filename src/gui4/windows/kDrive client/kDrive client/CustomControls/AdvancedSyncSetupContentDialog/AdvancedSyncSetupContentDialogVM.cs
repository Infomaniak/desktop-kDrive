using DynamicData;
using Infomaniak.kDrive.ViewModels;
using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using static Infomaniak.kDrive.CustomControls.AdvancedSyncSetupContentDialog;

namespace Infomaniak.kDrive.CustomControls
{
    public partial class AdvancedSyncSetupContentDialogVM : UISafeObservableObject
    {
        private NewSync? _newSync;
        public NewSync? NewSync
        {
            get => _newSync;
            set
            {
                SetPropertyInUIThread(ref _newSync, value);
                PreviousNewSyncState = value is not null ? new NewSync(value) : null;
            }
        }

        public NewSync? PreviousNewSyncState
        {
            get; set;
        }

        public AdvancedSyncSetupContentDialogVM(Drive drive)
        {
            _newSync = new NewSync() { Drive = drive };
            PreviousNewSyncState = new NewSync(_newSync);
        }

        // ContentDialogButtonClickEventHandler
        public event EventHandler? CurrentStepConfirmed;
        public event EventHandler? CurrentStepCancelled;
        public event EventHandler<AdvancedSyncSetupResult> SetupFinished = delegate { };

        public void ConfirmCurrentStep()
        {
            CurrentStepConfirmed?.Invoke(this, EventArgs.Empty);
        }

        public void CancelCurrentStep()
        {
            CurrentStepCancelled?.Invoke(this, EventArgs.Empty);
        }

        public void FinishSetup(AdvancedSyncSetupResult result)
        {
            SetupFinished?.Invoke(this, result);
        }

        public async Task RevertCurrentSyncChanges()
        {
            if (NewSync is not null && PreviousNewSyncState is not null)
            {
                NewSync.RemotePath = PreviousNewSyncState.RemotePath;
                NewSync.LocalPath = PreviousNewSyncState.LocalPath;
                NewSync.SyncType = PreviousNewSyncState.SyncType;
                NewSync.RemoteNodeId = PreviousNewSyncState.RemoteNodeId;
                NewSync.ExcludedNodeIds.Clear();
                NewSync.ExcludedNodeIds.AddRange(PreviousNewSyncState.ExcludedNodeIds);
                await NewSync.SelectBestVfsMode();
            }
            else
            {
                Logger.Log(Logger.Level.Warning, $"CurrentSync or PreviousCurrentSyncState is null. Cannot revert changes for the current sync - CurrentSync: {NewSync}, PreviousCurrentSyncState: {PreviousNewSyncState}");
            }
        }
    }
}
