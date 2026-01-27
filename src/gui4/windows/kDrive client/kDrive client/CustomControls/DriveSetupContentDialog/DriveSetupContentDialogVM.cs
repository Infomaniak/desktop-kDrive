using Infomaniak.kDrive.ViewModels;
using System;
using System.Collections.Generic;
using System.Linq;

namespace Infomaniak.kDrive.CustomControls
{
    public partial class DriveSetupContentDialogVM : UISafeObservableObject
    {

        private IList<NewSync> _newSyncs;
        public IList<NewSync> NewSyncs
        {
            get => _newSyncs;
        }

        private NewSync? _currentSync;
        public NewSync? CurrentSync
        {
            get => _currentSync;
            set => SetPropertyInUIThread(ref _currentSync, value);
        }

        public DriveSetupContentDialogVM(IList<NewSync> newSyncs)
        {
            _newSyncs = newSyncs;
        }

        // ContentDialogButtonClickEventHandler
        public event EventHandler? CurrentStepConfirmed;
        public event EventHandler? CurrentStepCancelled;
        public event EventHandler? SetupFinished;


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

        public void FinishSetup()
        {
            SetupFinished?.Invoke(this, EventArgs.Empty);
        }
    }
}
