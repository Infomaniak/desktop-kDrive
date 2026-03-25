using DynamicData.Binding;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml.Controls;
using System;
using System.Linq;

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class HomeErrorInfoBar : UserControl
    {
        public HomeErrorInfoBarViewModel ViewModel { get; } = new HomeErrorInfoBarViewModel();

        public HomeErrorInfoBar()
        {
            this.InitializeComponent();

            this.Unloaded += (s, e) => ViewModel.Dispose();
        }

        private void kDriveFullInfoBar_Closed(InfoBar sender, InfoBarClosedEventArgs args)
        {

        }
    }

    public class HomeErrorInfoBarViewModel : UISafeObservableObject, IDisposable
    {
        private bool _showDriveFullError = false;
        private bool _showTmpDirAccessError = false;
        private bool _showNetworkTimeOutError = false;
        private IDisposable? _appErrorsSubscriptions;
        private IDisposable? _selectedSyncErrorsSubscriptions;
        private bool _isDisposed = false;
        private AppModel ViewModel { get; } = App.ServiceProvider.GetRequiredService<AppModel>();

        public HomeErrorInfoBarViewModel()
        {
            // Subscribe to changes in the app errors and selected sync to update the error display
            _appErrorsSubscriptions = ViewModel.AppErrors.ToObservableChangeSet().Subscribe(_ => Refresh());

            ViewModel.SelectedSyncChanged += ViewModel_SelectedSyncChanged;
        }

        public void Dispose()
        {
            if (!_isDisposed)
            {
                _appErrorsSubscriptions?.Dispose();
                _selectedSyncErrorsSubscriptions?.Dispose();
                ViewModel.SelectedSyncChanged -= ViewModel_SelectedSyncChanged;
                _isDisposed = true;
            }
        }


        private void ViewModel_SelectedSyncChanged(object? sender, AppModel.SelectedSyncChangedEventArgs e)
        {
            _selectedSyncErrorsSubscriptions?.Dispose();

            if (ViewModel.SelectedSync is not null)
                _selectedSyncErrorsSubscriptions = ViewModel.SelectedSync.SyncErrors.ToObservableChangeSet().Subscribe(_ => Refresh());
        }

        public bool ShowDriveFullError
        {
            get => _showDriveFullError;
            set => SetPropertyInUIThread(ref _showDriveFullError, value);
        }
        public bool ShowTmpDirAccessError
        {
            get => _showTmpDirAccessError;
            set => SetPropertyInUIThread(ref _showTmpDirAccessError, value);
        }
        public bool ShowNetworkTimeOutError
        {
            get => _showNetworkTimeOutError;
            set => SetPropertyInUIThread(ref _showNetworkTimeOutError, value);
        }

        public void ResolveDriveFullError()
        {

        }

        private void Refresh()
        {
            Action resetErrors = () =>
            {
                ShowDriveFullError = false;
                ShowTmpDirAccessError = false;
                ShowNetworkTimeOutError = false;
            };

            if (HasTmpDirAccessError())
            {
                if (!ShowTmpDirAccessError)
                {
                    resetErrors();
                    ShowTmpDirAccessError = true;
                }
            }
            else if (HasNetworkTimeOutError())
            {
                if (!ShowNetworkTimeOutError)
                {
                    resetErrors();
                    ShowNetworkTimeOutError = true;
                }
            }
            else if (HasDriveFullError())
            {
                if (!ShowDriveFullError)
                {
                    resetErrors();
                    ShowDriveFullError = true;
                }
            }
            else
            {
                resetErrors();
            }
        }

        private bool HasTmpDirAccessError() =>
            ViewModel.AppErrors.Any(e => e.ExitCause == Types.ExitCause.TmpDirAccessError);

        private bool HasDriveFullError() =>
            ViewModel.SelectedSync?.SyncErrors.Any(e => e.ExitCause == Types.ExitCause.QuotaExceeded) ?? false;

        private bool HasNetworkTimeOutError() =>
            ViewModel.AppErrors.Any(e => e.ExitCause == Types.ExitCause.NetworkTimeout) ||
            ViewModel.SelectedSync?.SyncErrors.Any(e => e.ExitCause == Types.ExitCause.NetworkTimeout) == true;
    }
}
