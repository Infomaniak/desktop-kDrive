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
using DynamicData.Binding;
using Infomaniak.kDrive.Analytics;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml.Controls;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Windows.System;

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class HomeErrorInfoBar : UserControl
    {
        private readonly IAnalyticsService _analyticsService = App.ServiceProvider.GetRequiredService<IAnalyticsService>();
        public HomeErrorInfoBarViewModel ControlViewModel { get; } = new HomeErrorInfoBarViewModel();
        private AppModel AppViewModel { get; } = App.ServiceProvider.GetRequiredService<AppModel>();

        public HomeErrorInfoBar()
        {
            InitializeComponent();
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

        private async void kDriveFullInfoBar_Click(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            if (AppViewModel.SelectedSync is null)
            {
                return;
            }
            _analyticsService.TrackClick(Analytics.Keys.Category.HomePage, Analytics.Keys.EventName.OpenStorage);
            Uri changeOfferUri = App.Constants.Drive.ChangeOfferUri(AppViewModel.SelectedSync.Drive.DriveId);
            await Launcher.LaunchUriAsync(changeOfferUri);
        }

        private void TmpDirAccessError_Click(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            _analyticsService.TrackClick(Analytics.Keys.Category.HomePage, Analytics.Keys.EventName.RestartApp);
            App.ExitApplicationAndShutdownServer();
        }
    }

    public class HomeErrorInfoBarViewModel : UISafeObservableObject, IDisposable
    {
        private bool _showDriveFullError = false;
        private bool _showTmpDirAccessError = false;
        private IDisposable? _appErrorsSubscriptions;
        private IDisposable? _selectedSyncErrorsSubscriptions;
        private bool _isDisposed = false;
        private AppModel ViewModel { get; } = App.ServiceProvider.GetRequiredService<AppModel>();

        public HomeErrorInfoBarViewModel()
        {
            // Subscribe to changes in the app errors and selected sync to update the error display
            _appErrorsSubscriptions = ViewModel.AppErrors.ToObservableChangeSet().Subscribe(_ => Refresh());
            ViewModel.SelectedSyncChanged += ViewModel_SelectedSyncChanged;
            if (ViewModel.SelectedSync is not null)
            {
                ViewModel.SelectedSync.Drive.PropertyChanged += SelectedSyncDrive_PropertyChanged;
                _selectedSyncErrorsSubscriptions = ViewModel.SelectedSync.SyncErrors.ToObservableChangeSet().Subscribe(_ => Refresh());
            }
            Refresh();
        }

        public void Dispose()
        {
            if (!_isDisposed)
            {
                _appErrorsSubscriptions?.Dispose();
                _selectedSyncErrorsSubscriptions?.Dispose();
                ViewModel.SelectedSyncChanged -= ViewModel_SelectedSyncChanged;
                if (ViewModel.SelectedSync is not null)
                    ViewModel.SelectedSync.Drive.PropertyChanged -= SelectedSyncDrive_PropertyChanged;
                _isDisposed = true;
            }
        }


        private void ViewModel_SelectedSyncChanged(object? sender, AppModel.SelectedSyncChangedEventArgs e)
        {
            _selectedSyncErrorsSubscriptions?.Dispose();

            if (e.OldValue is not null)
                e.OldValue.Drive.PropertyChanged -= SelectedSyncDrive_PropertyChanged;

            if (e.NewValue is not null)
            {
                _selectedSyncErrorsSubscriptions = e.NewValue.SyncErrors.ToObservableChangeSet().Subscribe(_ => Refresh());
                e.NewValue.Drive.PropertyChanged += SelectedSyncDrive_PropertyChanged;
            }

            Refresh();
        }

        private void SelectedSyncDrive_PropertyChanged(object? sender, System.ComponentModel.PropertyChangedEventArgs e)
        {
            if (e.PropertyName == nameof(Drive.DisplayRemoteSpaceWarning))
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

        public async Task<bool> ResolveDriveFullError()
        {
            if (ViewModel.SelectedSync is null)
                return false;

            ViewModel.SelectedSync.Drive.DisplayRemoteSpaceWarning = false;
            return true;
        }

        public async Task<bool> ResolveTmpDirAccessError()
        {
            List<Error> tmpDirAccessErrorsList = ViewModel.AppErrors.Where(e => e.ExitCause == Types.ExitCause.TmpDirAccessError).ToList();
            var commService = App.ServiceProvider.GetRequiredService<ServerCommunication.Interfaces.IServerCommService>();
            bool result = true;
            foreach (var error in tmpDirAccessErrorsList)
            {
                result &= await commService.DeleteError(error.DbId, CancellationToken.None);
            }
            return result;
        }

        private void HideAll()
        {

            ShowDriveFullError = false;
            ShowTmpDirAccessError = false;
        }
        public void Refresh()
        {

            if (HasTmpDirAccessError())
            {
                if (!ShowTmpDirAccessError)
                {
                    HideAll();
                    ShowTmpDirAccessError = true;
                }
            }
            else if (HasDriveFullError())
            {
                if (!ShowDriveFullError)
                {
                    HideAll();
                    ShowDriveFullError = true;
                }
            }
            else
            {
                HideAll();
            }
        }

        private bool HasTmpDirAccessError()
        {
            bool result = ViewModel.AppErrors.Any(e => e.ExitCause == Types.ExitCause.TmpDirAccessError);

            if (result || ViewModel.SelectedSync is null)
                return result;

            return ViewModel.SelectedSync.SyncErrors.Any(e => e.ExitCause == Types.ExitCause.TmpDirAccessError);
        }

        private bool HasDriveFullError() =>
            ViewModel.SelectedSync?.Drive.DisplayRemoteSpaceWarning ?? false;
    }
}
