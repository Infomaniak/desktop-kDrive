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
using Infomaniak.kDrive.Pages.AdvancedSyncSetupContentDialog;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System.ComponentModel;

namespace Infomaniak.kDrive.CustomControls
{
    public partial class AdvancedSyncSetupContentDialog : ContentDialog
    {
        public enum AdvancedSyncSetupResult
        {
            Confirmed,
            Cancelled
        }

        private readonly AdvancedSyncSetupContentDialogVM _advancedSyncSetupContentDialogVM;
        public AdvancedSyncSetupResult Result { get; private set; } = AdvancedSyncSetupResult.Cancelled;

        public AdvancedSyncSetupContentDialog(XamlRoot xamlRoot, IDrive drive)
        {
            _advancedSyncSetupContentDialogVM = new AdvancedSyncSetupContentDialogVM(drive);
            base.MinWidth = 540;
            base.DataContext = _advancedSyncSetupContentDialogVM;
            base.XamlRoot = xamlRoot;
            base.PrimaryButtonText = Localizer.Instance.GetString("buttonValidate");
            base.IsPrimaryButtonEnabled = false;
            base.CloseButtonText = Localizer.Instance.GetString("buttonCancel");
            base.DefaultButton = ContentDialogButton.Primary;
            var frame = new Frame();
            base.Content = frame;
            frame.Navigate(typeof(SyncSetupPage), _advancedSyncSetupContentDialogVM);
            base.PrimaryButtonClick += AdvancedSyncSetupContentDialog_PrimaryButtonClick;
            base.CloseButtonClick += AdvancedSyncSetupContentDialog_CloseButtonClick;
            _advancedSyncSetupContentDialogVM.SetupFinished += AdvancedSyncSetupContentDialogVM_SetupFinished;
            _advancedSyncSetupContentDialogVM.PropertyChanged += AdvancedSyncSetupContentDialogVM_PropertyChanged;
        }

        public NewSync? GetNewSync()
        {
            if (Result != AdvancedSyncSetupResult.Confirmed)
                return null;
            return _advancedSyncSetupContentDialogVM.NewSync;
        }

        private void AdvancedSyncSetupContentDialogVM_PropertyChanged(object? sender, PropertyChangedEventArgs e)
        {
            if (e.PropertyName == nameof(AdvancedSyncSetupContentDialogVM.CanGoNext))
                base.IsPrimaryButtonEnabled = _advancedSyncSetupContentDialogVM.CanGoNext;
        }

        private void AdvancedSyncSetupContentDialogVM_SetupFinished(object? sender, AdvancedSyncSetupResult e)
        {
            Result = e;
            base.Hide();
        }

        // Intercept button clicks to manage steps
        private void AdvancedSyncSetupContentDialog_PrimaryButtonClick(ContentDialog sender, ContentDialogButtonClickEventArgs args)
        {
            _advancedSyncSetupContentDialogVM.ConfirmCurrentStep();
            args.Cancel = true;
        }

        private void AdvancedSyncSetupContentDialog_CloseButtonClick(ContentDialog sender, ContentDialogButtonClickEventArgs args)
        {
            _advancedSyncSetupContentDialogVM.CancelCurrentStep();
            args.Cancel = true;
        }
    }
}
