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
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.Collections.Generic;
using System.ComponentModel;

namespace Infomaniak.kDrive.Pages
{
    public sealed partial class HomePage : Page
    {
        private readonly IAnalyticsService _analyticsService = App.ServiceProvider.GetRequiredService<IAnalyticsService>();
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

        private bool RedirectToErrorPageIfNeeded()
        {
            if (ViewModel.SelectedSync is null)
                return false;

            switch (ViewModel.SelectedSync.SyncErrorState)
            {
                case SyncErrorStates.Undefined:
                    // No error, stay on the HomePage
                    return false;
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
                    return false;
            }
            return true;
        }

        protected override async void OnNavigatedTo(NavigationEventArgs e)
        {
            ViewModel.SelectedSyncChanged += OnSelectedSyncChanged;
            OnSelectedSyncChanged(null, new(null, ViewModel.SelectedSync));
            if (!RedirectToErrorPageIfNeeded())
                _analyticsService.TrackPageView(Analytics.Keys.Category.HomePage);
        }

        protected override void OnNavigatedFrom(NavigationEventArgs e)
        {
            DetachHandlers();
            CleanupLottiePlayers();
        }

        private void CleanupLottiePlayers()
        {
            try
            {
                // Find and cleanup all LottiePlayer controls in the visual tree
                var lottiePlayers = FindVisualChildren<CustomControls.LottiePlayer>(this);
                foreach (var player in lottiePlayers)
                {
                    player?.Cleanup();
                }
            }
            catch (Exception ex)
            {
                Logger.Log(Logger.Level.Warning, $"Error cleaning up Lottie players: {ex.Message}");
            }
        }

        private static IEnumerable<T> FindVisualChildren<T>(DependencyObject parent) where T : DependencyObject
        {
            if (parent == null)
                yield break;

            int childCount = Microsoft.UI.Xaml.Media.VisualTreeHelper.GetChildrenCount(parent);
            for (int i = 0; i < childCount; i++)
            {
                var child = Microsoft.UI.Xaml.Media.VisualTreeHelper.GetChild(parent, i);
                if (child is T typedChild)
                {
                    yield return typedChild;
                }

                foreach (var descendant in FindVisualChildren<T>(child))
                {
                    yield return descendant;
                }
            }
        }

        private void DetachHandlers()
        {
            ViewModel.SelectedSyncChanged -= OnSelectedSyncChanged;

            if (ViewModel.SelectedSync is not null)
                ViewModel.SelectedSync.PropertyChanged -= OnSelectedSyncPropertyChanged;
        }

        private void SyncInProgressHyperlinkButton_Click(object sender, RoutedEventArgs e)
        {
            _analyticsService.TrackClick(Analytics.Keys.Category.HomePage, Analytics.Keys.EventName.OpenActivity);
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
            _analyticsService.TrackClick(Analytics.Keys.Category.HomePage, Analytics.Keys.EventName.StartSync);
            await ViewModel.SelectedSync.Start();
        }

        public string GetGreetingLabel(string? userName, SyncStatus? status)
        {
            if (userName is null || status is null)
                return Localizer.Instance.GetString("labelWelcomeToKDrive");


            const string transitionStr = "…";
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
            _analyticsService.TrackClick(Analytics.Keys.Category.HomePage, Analytics.Keys.EventName.OpenLoginWeb);
            (App.Current as App)?.StartOnboarding();
        }
    }
}
