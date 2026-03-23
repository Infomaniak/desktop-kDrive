using DynamicData;
using Infomaniak.kDrive.ViewModels;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using static Infomaniak.kDrive.CustomControls.DriveSetupContentDialog;

namespace Infomaniak.kDrive.CustomControls
{
    public partial class DriveSetupContentDialogVM : UISafeObservableObject
    {
        private readonly IList<NewSync> _initialState;

        private readonly IList<NewSync> _newSyncs;
        public IList<NewSync> NewSyncs
        {
            get => _newSyncs;
        }

        private NewSync? _currentSync;
        public NewSync? CurrentSync
        {
            get => _currentSync;
            set
            {
                SetPropertyInUIThread(ref _currentSync, value);
                PreviousCurrentSyncState = value is not null ? new NewSync(value) : null;
            }
        }

        public NewSync? PreviousCurrentSyncState
        {
            get; set;
        }

        public DriveSetupContentDialogVM(IList<NewSync> newSyncs)
        {
            _initialState = [.. newSyncs.Select(ns => new NewSync(ns))];
            _newSyncs = newSyncs;
        }

        // ContentDialogButtonClickEventHandler
        public event EventHandler? CurrentStepConfirmed;
        public event EventHandler? CurrentStepCancelled;
        public event EventHandler<DriveSetupResult> SetupFinished = delegate { };

        public bool IsMultipleDrivesSetup()
        {
            return NewSyncs.Count() > 1;
        }

        public void ConfirmCurrentStep()
        {
            CurrentStepConfirmed?.Invoke(this, EventArgs.Empty);
        }

        public void CancelCurrentStep()
        {
            CurrentStepCancelled?.Invoke(this, EventArgs.Empty);
        }

        public void FinishSetup(DriveSetupResult result)
        {
            SetupFinished?.Invoke(this, result);
        }

        public async Task RevertAllChanges()
        {
            foreach (var sync in NewSyncs)
            {
                var initialSync = _initialState.FirstOrDefault(ns => ns.Drive == sync.Drive);
                if (initialSync is not null)
                {
                    sync.RemotePath = initialSync.RemotePath;
                    sync.LocalPath = initialSync.LocalPath;
                    sync.SyncType = initialSync.SyncType;
                    sync.RemoteNodeId = initialSync.RemoteNodeId;
                    sync.ExcludedNodeIds.Clear();
                    sync.ExcludedNodeIds.AddRange(initialSync.ExcludedNodeIds);
                    await sync.SelectBestVfsMode();
                }
                else
                {
                    Logger.Log(Logger.Level.Warning, $"No initial state found for sync with DriveId {sync.Drive?.DriveId}. Cannot revert changes for this sync.");
                }
            }
        }

        public async Task RevertCurrentSyncChanges()
        {
            if (CurrentSync is not null && PreviousCurrentSyncState is not null)
            {
                CurrentSync.RemotePath = PreviousCurrentSyncState.RemotePath;
                CurrentSync.LocalPath = PreviousCurrentSyncState.LocalPath;
                CurrentSync.SyncType = PreviousCurrentSyncState.SyncType;
                CurrentSync.RemoteNodeId = PreviousCurrentSyncState.RemoteNodeId;
                CurrentSync.ExcludedNodeIds.Clear();
                CurrentSync.ExcludedNodeIds.AddRange(PreviousCurrentSyncState.ExcludedNodeIds);
                await CurrentSync.SelectBestVfsMode();
            }
            else
            {
                Logger.Log(Logger.Level.Warning, $"CurrentSync or PreviousCurrentSyncState is null. Cannot revert changes for the current sync - CurrentSync: {CurrentSync}, PreviousCurrentSyncState: {PreviousCurrentSyncState}");
            }
        }
    }
}
