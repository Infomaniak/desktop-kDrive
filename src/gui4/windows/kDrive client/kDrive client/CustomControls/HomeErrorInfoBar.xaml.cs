using DynamicData.Binding;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml.Controls;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class HomeErrorInfoBar : UserControl
    {
        public HomeErrorInfoBarViewModel ControlViewModel { get; } = new HomeErrorInfoBarViewModel();
        private AppModel AppViewModel { get; } = App.ServiceProvider.GetRequiredService<AppModel>();

        public HomeErrorInfoBar()
        {
            this.InitializeComponent();

            this.Unloaded += (s, e) => ControlViewModel.Dispose();
        }

        private async void kDriveFullInfoBar_Closed(InfoBar sender, InfoBarClosedEventArgs args)
        {
            if (!await ControlViewModel.ResolveDriveFullError())
            {
                Logger.Log(Logger.Level.Error, "Failed to resolve drive full error");
                Utility.ShowUnexpectedErrorTeachingTip();
            }
        }

        private async void TmpDirAccessError_Closed(InfoBar sender, InfoBarClosedEventArgs args)
        {
            if (!await ControlViewModel.ResolveTmpDirAccessError())
            {
                Logger.Log(Logger.Level.Error, "Failed to resolve tmp dir access error");
                Utility.ShowUnexpectedErrorTeachingTip();
            }
        }

        private async void NetworkTimeOutError_Closed(InfoBar sender, InfoBarClosedEventArgs args)
        {
            if (!await ControlViewModel.ResolveNetworkTimeOutError())
            {
                Logger.Log(Logger.Level.Error, "Failed to resolve network timeout error");
                Utility.ShowUnexpectedErrorTeachingTip();
            }
        }

        private void kDriveFullInfoBar_Click(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {

        }

        private void TmpDirAccessError_Click(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            App.ExitApplicationAndShutdownServer();
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
            Refresh();
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

            if(e.OldValue is not null)
                e.OldValue.PropertyChanged -= SelectedSync_PropertyChanged;

            if (e.NewValue is not null)
            {
                _selectedSyncErrorsSubscriptions = e.NewValue.SyncErrors.ToObservableChangeSet().Subscribe(_ => Refresh());
                e.NewValue.PropertyChanged += SelectedSync_PropertyChanged;
            }

            Refresh();
        }

        private void SelectedSync_PropertyChanged(object? sender, System.ComponentModel.PropertyChangedEventArgs e)
        {
            if (e.PropertyName == nameof(Sync.Drive.DisplayRemoteSpaceWarning))
                Refresh();
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

        public async Task<bool> ResolveDriveFullError()
        {
            if (ViewModel.SelectedSync is null)
                return true;

            ViewModel.SelectedSync.Drive.DisplayRemoteSpaceWarning = false;
            return true;
        }

        public async Task<bool> ResolveTmpDirAccessError()
        {
            List<Error> tmpDirAccessErrorsList = ViewModel.AppErrors.Where(e => e.ExitCause == Types.ExitCause.TmpDirAccessError).ToList();
            var commSertvice = App.ServiceProvider.GetRequiredService<ServerCommunication.Interfaces.IServerCommService>();
            bool result = true;
            foreach (var error in tmpDirAccessErrorsList)
            {
                result = await commSertvice.DeleteError(error.DbId, CancellationToken.None);
            }
            return result;
        }

        public async Task<bool> ResolveNetworkTimeOutError()
        {
            List<Error> networkTimeOutErrorsList = ViewModel.AppErrors.Where(e => e.ExitCause == Types.ExitCause.NetworkTimeout).ToList();
            if (ViewModel.SelectedSync is not null)
                networkTimeOutErrorsList.AddRange(ViewModel.SelectedSync.SyncErrors.Where(e => e.ExitCause == Types.ExitCause.NetworkTimeout));
            var commSertvice = App.ServiceProvider.GetRequiredService<ServerCommunication.Interfaces.IServerCommService>();
            bool result = true;
            foreach (var error in networkTimeOutErrorsList)
            {
                result = await commSertvice.DeleteError(error.DbId, CancellationToken.None);
            }
            return result;
        }

        public void Refresh()
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
            ViewModel.SelectedSync?.Drive.DisplayRemoteSpaceWarning ?? false;

        private bool HasNetworkTimeOutError() =>
            ViewModel.AppErrors.Any(e => e.ExitCause == Types.ExitCause.NetworkTimeout) ||
            ViewModel.SelectedSync?.SyncErrors.Any(e => e.ExitCause == Types.ExitCause.NetworkTimeout) == true;
    }
}
