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

        private readonly DriveSetupContentDialogVM _driveSetupContentDialogVM;
        public DriveSetupResult Result { get; private set; } = DriveSetupResult.Cancelled;

        public DriveSetupContentDialog(XamlRoot xamlRoot, IList<NewSync> newSyncs)
        {
            _driveSetupContentDialogVM = new DriveSetupContentDialogVM(newSyncs);
            base.DataContext = _driveSetupContentDialogVM;
            base.XamlRoot = xamlRoot;
            base.PrimaryButtonText = Localizer.Instance.GetString("buttonConfirm");
            base.CloseButtonText = Localizer.Instance.GetString("buttonCancel");
            base.DefaultButton = ContentDialogButton.Primary;
            var frame = new Frame();
            base.Content = frame;
            frame.Navigate(typeof(DriveSelectionPage), _driveSetupContentDialogVM);
            base.PrimaryButtonClick += DriveSetupContentDialog_PrimaryButtonClick;
            base.CloseButtonClick += DriveSetupContentDialog_CloseButtonClick;
            _driveSetupContentDialogVM.SetupFinished += DriveSetupContentDialogVM_SetupFinished;
        }

        private void DriveSetupContentDialogVM_SetupFinished(object? sender, DriveSetupResult e)
        {
            Result = e;
            base.Hide();
        }

        // Intercept button clicks to manage steps
        private void DriveSetupContentDialog_CloseButtonClick(ContentDialog sender, ContentDialogButtonClickEventArgs args)
        {
            _driveSetupContentDialogVM.CancelCurrentStep();
            args.Cancel = true;
        }

        private void DriveSetupContentDialog_PrimaryButtonClick(ContentDialog sender, ContentDialogButtonClickEventArgs args)
        {
            _driveSetupContentDialogVM.ConfirmCurrentStep();
            args.Cancel = true;
        }
    }
}
