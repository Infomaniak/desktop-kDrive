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
using Infomaniak.kDrive.Analytics;
using Infomaniak.kDrive.Pages.DriveSetupContentDialog;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System.Collections.Generic;

namespace Infomaniak.kDrive.CustomControls
{
    public partial class DriveSetupContentDialog : ContentDialog
    {
        private readonly IAnalyticsService _analyticsService = App.ServiceProvider.GetRequiredService<IAnalyticsService>();
        public enum DriveSetupResult
        {
            Confirmed,
            Cancelled
        }

        private readonly DriveSetupContentDialogVM _driveSetupContentDialogVM;
        public DriveSetupResult Result { get; private set; } = DriveSetupResult.Cancelled;

        public DriveSetupContentDialog(XamlRoot xamlRoot, IList<NewSync> newSyncs)
        {
            _driveSetupContentDialogVM = new DriveSetupContentDialogVM(newSyncs);
            base.DataContext = _driveSetupContentDialogVM;
            base.XamlRoot = xamlRoot;
            base.PrimaryButtonText = Localizer.Instance.GetString("buttonValidate");
            base.CloseButtonText = Localizer.Instance.GetString("buttonCancel");
            base.DefaultButton = ContentDialogButton.Primary;
            var frame = new Frame();
            base.Content = frame;
            frame.Navigate(typeof(DriveSelectionPage), _driveSetupContentDialogVM);
            base.PrimaryButtonClick += DriveSetupContentDialog_PrimaryButtonClick;
            base.CloseButtonClick += DriveSetupContentDialog_CloseButtonClick;
            _driveSetupContentDialogVM.SetupFinished += DriveSetupContentDialogVM_SetupFinished;
            _analyticsService.TrackPageView(Analytics.Keys.Category.DriveSetupDialog);
        }

        private void DriveSetupContentDialogVM_SetupFinished(object? sender, DriveSetupResult e)
        {
            _driveSetupContentDialogVM.SetupFinished -= DriveSetupContentDialogVM_SetupFinished;
            Result = e;
            base.Hide();
        }

        // Intercept button clicks to manage steps
        private void DriveSetupContentDialog_CloseButtonClick(ContentDialog sender, ContentDialogButtonClickEventArgs args)
        {
            _analyticsService.TrackClick(Analytics.Keys.Category.DriveSetupDialog, Analytics.Keys.EventName.Cancel);
            _driveSetupContentDialogVM.CancelCurrentStep();
            args.Cancel = true;
        }

        private void DriveSetupContentDialog_PrimaryButtonClick(ContentDialog sender, ContentDialogButtonClickEventArgs args)
        {
            _analyticsService.TrackClick(Analytics.Keys.Category.DriveSetupDialog, Analytics.Keys.EventName.Confirm);
            _driveSetupContentDialogVM.ConfirmCurrentStep();
            args.Cancel = true;
        }
    }
}
