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
        private readonly AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
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
            if (ViewModel.SelectedSync is null)
                return;

            switch (ViewModel.SelectedSync.SyncErrorState)
            {
                case SyncErrorStates.Undefined:
                    // No error, stay on the HomePage
                    break;
                case SyncErrorStates.AccessDenied:
                    AppModel.UIThreadDispatcher.TryEnqueue(() => Frame?.Navigate(typeof(DriveAccessDeniedPage)));
                    break;
                case SyncErrorStates.LoggingError:
                    AppModel.UIThreadDispatcher.TryEnqueue(() => Frame?.Navigate(typeof(LogginErrorPage)));
                    break;
                case SyncErrorStates.NotRenew:
                    AppModel.UIThreadDispatcher.TryEnqueue(() => Frame?.Navigate(typeof(NotRenewErrorPage)));
                    break;
                case SyncErrorStates.Maintenance:
                    AppModel.UIThreadDispatcher.TryEnqueue(() => Frame?.Navigate(typeof(MaintenanceErrorPage)));
                    break;
                case SyncErrorStates.Asleep:
                    AppModel.UIThreadDispatcher.TryEnqueue(() => Frame?.Navigate(typeof(AsleepErrorPage)));
                    break;
                default:
                    Logger.Log(Logger.Level.Warning, $"Unexpected SyncErrorState: {ViewModel.SelectedSync?.SyncErrorState}. Staying on HomePage.");
                    break;
            }
        }

        protected override async void OnNavigatedTo(NavigationEventArgs e)
        {
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

        private void SyncInProgressHyperlinkButton_Click(object sender, RoutedEventArgs e)
        {
            DetachHandlers();
            Frame.Navigate(typeof(ActivityPage));
        }

        private async void RestartButton_Click(object sender, RoutedEventArgs e)
        {
            if (ViewModel.SelectedSync is null)
            {
                Logger.Log(Logger.Level.Warning, "No sync is selected, cannot resume sync.");
                return;
            }
            await ViewModel.SelectedSync.Start();
        }

        public string GetGreetingLabel(string? userName, SyncStatus? status)
        {
            if(userName is null || status is null)
                return Localizer.Instance.GetString("labelWelcomeToKDrive");


            const string transitionStr = "...";
            string syncStateStr = status switch
            {
                SyncStatus.Undefined => transitionStr,
                SyncStatus.Starting => transitionStr,
                SyncStatus.Running => Localizer.Instance.GetString("synchroInProgress"),
                SyncStatus.Idle => Localizer.Instance.GetString("synchroUpToDate"),
                SyncStatus.PauseAsked => transitionStr,
                SyncStatus.Paused => Localizer.Instance.GetString("synchroPaused"),
                SyncStatus.StopAsked => transitionStr,
                SyncStatus.Stopped => Localizer.Instance.GetString("synchroPaused"),
                SyncStatus.Error => transitionStr,
                SyncStatus.Offline => Localizer.Instance.GetString("synchroPaused"),
                _ => Localizer.Instance.GetString("labelWelcomeToKDrive")
            };

            return Localizer.Instance.GetString("greetingLabel", userName, syncStateStr);
        }

        private void StartOnboardingButton_Click(object sender, RoutedEventArgs e)
        {
            (App.Current as App)?.StartOnboarding();
        }
    }
}
