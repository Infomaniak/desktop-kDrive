using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using System;
using static Infomaniak.kDrive.CustomControls.AdvancedSyncSetupContentDialog;

namespace Infomaniak.kDrive.CustomControls
{
    public partial class AdvancedSyncSetupContentDialogVM : UISafeObservableObject
    {
        private NewSync _newSync;
        private bool _canGoNext = true;

        public NewSync NewSync
        {
            get => _newSync;
            set => SetPropertyInUIThread(ref _newSync, value);
        }

        public bool CanGoNext
        {
            get => _canGoNext;
            set => SetPropertyInUIThread(ref _canGoNext, value);
        }

        public AdvancedSyncSetupContentDialogVM(IDrive drive)
        {
            _newSync = new NewSync() { Drive = drive };
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
    }
}
