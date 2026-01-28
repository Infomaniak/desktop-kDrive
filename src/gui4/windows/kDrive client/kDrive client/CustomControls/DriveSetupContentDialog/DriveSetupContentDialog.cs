using Infomaniak.kDrive.Pages.DriveSetupContentDialog;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System.Collections.Generic;

namespace Infomaniak.kDrive.CustomControls
{
    public partial class DriveSetupContentDialog : ContentDialog
    {
        public enum DriveSetupResult
        {
            Confirmed,
            Cancelled
        }

        private DriveSetupContentDialogVM _driveSetupContentDialogVM;
        public DriveSetupResult Result { get; private set; }

        public DriveSetupContentDialog(XamlRoot xamlRoot, IList<NewSync> newSyncs)
        {
            _driveSetupContentDialogVM = new DriveSetupContentDialogVM(newSyncs);
            base.DataContext = _driveSetupContentDialogVM;
            base.XamlRoot = xamlRoot;
            base.SecondaryButtonText = Utility.GetLocalizedString("CC_DriveSetupContentDialog_ConfirmButton/Content");
            base.PrimaryButtonText = Utility.GetLocalizedString("CC_DriveSetupContentDialog_CancelButton/Content");
            base.DefaultButton = ContentDialogButton.Secondary;
            var frame = new Frame();
            base.Content = frame;
            frame.Navigate(typeof(DriveSelectionPage), _driveSetupContentDialogVM);
            base.PrimaryButtonClick += DriveSetupContentDialog_PrimaryButtonClick;
            base.SecondaryButtonClick += DriveSetupContentDialog_SecondaryButtonClick;
            _driveSetupContentDialogVM.SetupFinished += DriveSetupContentDialogVM_SetupFinished;
        }

        private void DriveSetupContentDialogVM_SetupFinished(object? sender, DriveSetupResult e)
        {
            Result = e;
            base.Hide();
        }

        // Intercept button clicks to manage steps
        private void DriveSetupContentDialog_SecondaryButtonClick(ContentDialog sender, ContentDialogButtonClickEventArgs args)
        {
            _driveSetupContentDialogVM.ConfirmCurrentStep();
            args.Cancel = true;
        }

        private void DriveSetupContentDialog_PrimaryButtonClick(ContentDialog sender, ContentDialogButtonClickEventArgs args)
        {
            _driveSetupContentDialogVM.CancelCurrentStep();
            args.Cancel = true;
        }
    }
}
