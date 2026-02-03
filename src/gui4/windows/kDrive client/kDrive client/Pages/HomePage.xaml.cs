/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
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

using Infomaniak.kDrive.Pages.Settings;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.ComponentModel;

namespace Infomaniak.kDrive.Pages
{
    public sealed partial class HomePage : Page
    {
        private AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        public AppModel ViewModel => _viewModel;
        public HomePage()
        {
            Logger.Log(Logger.Level.Info, "Navigated to HomePage - Initializing HomePage components");
            InitializeComponent();
            Logger.Log(Logger.Level.Debug, "HomePage components initialized");
        }

        private void OnSelectedSyncChanged(object? sender, AppModel.SelectedSyncChangedEventArgs e)
        {
            if (e.OldValue is not null)
                e.OldValue.PropertyChanged -= OnSelectedSyncPropertyChanged;

            if (e.NewValue is not null)
                e.NewValue.PropertyChanged += OnSelectedSyncPropertyChanged;

            RedirectToErrorPageIfNeeded();
        }

        private void OnSelectedSyncPropertyChanged(object? sender, PropertyChangedEventArgs e)
        {
            if (e.PropertyName == nameof(Sync.SyncErrorState))
            {
                RedirectToErrorPageIfNeeded();
            }
        }

        private void RedirectToErrorPageIfNeeded()
        {
            switch (ViewModel.SelectedSync?.SyncErrorState)
            {
                case SyncErrorStates.Undefined:
                    // No error, stay on the HomePage
                    break;
                case SyncErrorStates.AccessDenied:
                    AppModel.UIThreadDispatcher.TryEnqueue(() => Frame.Navigate(typeof(DriveAccessDeniedPage)));
                    break;
                case SyncErrorStates.LoggingError:
                    AppModel.UIThreadDispatcher.TryEnqueue(() => Frame.Navigate(typeof(LoggingErrorPage)));
                    break;
                case SyncErrorStates.NotRenew:
                    AppModel.UIThreadDispatcher.TryEnqueue(() => Frame.Navigate(typeof(NotRenewErrorPage)));
                    break;
                case SyncErrorStates.Maintenance:
                    AppModel.UIThreadDispatcher.TryEnqueue(() => Frame.Navigate(typeof(MaintenanceErrorPage)));
                    break;
                case SyncErrorStates.Asleep:
                    AppModel.UIThreadDispatcher.TryEnqueue(() => Frame.Navigate(typeof(AsleepErrorPage)));
                    break;
                default:
                    Logger.Log(Logger.Level.Warning, $"Unexpected SyncErrorState: {ViewModel.SelectedSync?.SyncErrorState}. Staying on HomePage.");
                    break;
            }
        }

        protected override async void OnNavigatedTo(NavigationEventArgs e)
        {
            if (ViewModel.SelectedSync is null)
            {
                AppModel.UIThreadDispatcher.TryEnqueue(() =>
                {
                    DetachHandlers();
                    Frame.Navigate(typeof(SettingsPage));
                });
                return;
            }
            ViewModel.SelectedSyncChanged += OnSelectedSyncChanged;
            OnSelectedSyncChanged(null, new(null, ViewModel.SelectedSync));
            RedirectToErrorPageIfNeeded();
        }

        protected override void OnNavigatedFrom(NavigationEventArgs e)
        {
            DetachHandlers();
        }

        private void DetachHandlers()
        {
            ViewModel.SelectedSyncChanged -= OnSelectedSyncChanged;

            if (ViewModel.SelectedSync is not null)
                ViewModel.SelectedSync.PropertyChanged -= OnSelectedSyncPropertyChanged;
        }

        private void SyncUpToDateHyperlinkButton_Click(object sender, RoutedEventArgs e)
        {
            ((App)Application.Current).CurrentWindow?.AppWindow.Hide();
        }

        private void SyncInProgressHyperlinkButton_Click(object sender, RoutedEventArgs e)
        {
            DetachHandlers();
            Frame.Navigate(typeof(ActivityPage));
        }

        private void SyncInPauseHyperlinkButton_Click(object sender, RoutedEventArgs e)
        {
            if (ViewModel.SelectedSync == null)
            {
                Logger.Log(Logger.Level.Warning, "No sync is selected, cannot resume sync.");
                return;
            }
            ViewModel.SelectedSync.SyncStatus = SyncStatus.Running; // Todo: Replace with actual resume logic
        }
    }
}
