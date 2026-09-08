/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
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
            _newSync = new NewSync(drive);
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
