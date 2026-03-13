using Infomaniak.kDrive.Pages.AdvancedSyncSetupContentDialog;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

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
        public AdvancedSyncSetupResult Result { get; private set; }

        public AdvancedSyncSetupContentDialog(XamlRoot xamlRoot, Drive drive)
        {
            _advancedSyncSetupContentDialogVM = new AdvancedSyncSetupContentDialogVM(drive);
            base.MinWidth = 540;
            base.DataContext = _advancedSyncSetupContentDialogVM;
            base.XamlRoot = xamlRoot;
            base.SecondaryButtonText = Localizer.Instance.GetString("buttonConfirm");
            base.PrimaryButtonText = Localizer.Instance.GetString("buttonCancel");
            base.DefaultButton = ContentDialogButton.Secondary;
            var frame = new Frame();
            base.Content = frame;
            frame.Navigate(typeof(SyncSetupPage), _advancedSyncSetupContentDialogVM);
            base.PrimaryButtonClick += AdvancedSyncSetupContentDialog_PrimaryButtonClick;
            base.SecondaryButtonClick += AdvancedSyncSetupContentDialog_SecondaryButtonClick;
            _advancedSyncSetupContentDialogVM.SetupFinished += AdvancedSyncSetupContentDialogVM_SetupFinished;
        }

        private void AdvancedSyncSetupContentDialogVM_SetupFinished(object? sender, AdvancedSyncSetupResult e)
        {
            Result = e;
            base.Hide();
        }

        // Intercept button clicks to manage steps
        private void AdvancedSyncSetupContentDialog_SecondaryButtonClick(ContentDialog sender, ContentDialogButtonClickEventArgs args)
        {
            _advancedSyncSetupContentDialogVM.ConfirmCurrentStep();
            args.Cancel = true;
        }

        private void AdvancedSyncSetupContentDialog_PrimaryButtonClick(ContentDialog sender, ContentDialogButtonClickEventArgs args)
        {
            _advancedSyncSetupContentDialogVM.CancelCurrentStep();
            args.Cancel = true;
        }
    }
}
