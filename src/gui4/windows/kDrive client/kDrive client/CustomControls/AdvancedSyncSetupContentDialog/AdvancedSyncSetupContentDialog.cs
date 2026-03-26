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
            base.PrimaryButtonText = Localizer.Instance.GetString("buttonConfirm");
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
