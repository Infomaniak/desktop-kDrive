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
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml.Controls;
using System;
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
                Logger.LogError("Failed to resolve drive full error");
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
    }

    public class HomeErrorInfoBarViewModel : UISafeObservableObject, IDisposable
    {
        private bool _showDriveFullError = false;
        private bool _isDisposed = false;
        private AppModel ViewModel { get; } = App.ServiceProvider.GetRequiredService<AppModel>();

        public HomeErrorInfoBarViewModel()
        {
            // Follow the selected sync drive to update the error display
            ViewModel.SelectedSyncChanged += ViewModel_SelectedSyncChanged;
            if (ViewModel.SelectedSync is not null)
            {
                ViewModel.SelectedSync.Drive.PropertyChanged += SelectedSyncDrive_PropertyChanged;
            }
            Refresh();
        }

        public void Dispose()
        {
            if (!_isDisposed)
            {
                ViewModel.SelectedSyncChanged -= ViewModel_SelectedSyncChanged;
                if (ViewModel.SelectedSync is not null)
                    ViewModel.SelectedSync.Drive.PropertyChanged -= SelectedSyncDrive_PropertyChanged;
                _isDisposed = true;
            }
        }


        private void ViewModel_SelectedSyncChanged(object? sender, AppModel.SelectedSyncChangedEventArgs e)
        {
            if (e.OldValue is not null)
                e.OldValue.Drive.PropertyChanged -= SelectedSyncDrive_PropertyChanged;

            if (e.NewValue is not null)
                e.NewValue.Drive.PropertyChanged += SelectedSyncDrive_PropertyChanged;

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

        public async Task<bool> ResolveDriveFullError()
        {
            if (ViewModel.SelectedSync is null)
                return false;

            ViewModel.SelectedSync.Drive.DisplayRemoteSpaceWarning = false;
            return true;
        }

        private void HideAll()
        {

            ShowDriveFullError = false;
        }
        public void Refresh()
        {

            if (HasDriveFullError())
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

        private bool HasDriveFullError() =>
            ViewModel.SelectedSync?.Drive.DisplayRemoteSpaceWarning ?? false;
    }
}
