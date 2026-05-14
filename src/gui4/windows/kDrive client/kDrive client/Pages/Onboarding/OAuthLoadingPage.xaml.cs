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
using Infomaniak.kDrive.OnBoarding;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.ComponentModel;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;


namespace Infomaniak.kDrive.Pages.Onboarding
{
    public sealed partial class OAuthLoadingPage : Page
    {
        private static readonly TimeSpan _oauthTimeOut = TimeSpan.FromMinutes(5);
        private readonly IAnalyticsService _analyticsService = App.ServiceProvider.GetRequiredService<IAnalyticsService>();
        private readonly AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        private ViewModels.Onboarding? _onboardingViewModel;

        private Task? _onBoardingTask;
        private CancellationTokenSource _oauthCts = new(_oauthTimeOut);

        private Task? _enableRestartTask;
        private CancellationTokenSource _enableRestartCts = new();

        public AppModel ViewModel => _viewModel;

        public OAuthLoadingPage()
        {
            Logger.Log(Logger.Level.Info, "Navigated to OAuthLoadingPage - Initializing components");
            InitializeComponent();
            Logger.Log(Logger.Level.Debug, "OAuthLoadingPage components initialized");
        }
        protected override void OnNavigatedTo(NavigationEventArgs e)
        {
            base.OnNavigatedTo(e);

            if (e.Parameter is ViewModels.Onboarding obvm)
            {
                _onboardingViewModel = obvm;
                _onboardingViewModel.PropertyChanged += OnOnboardingViewModelPropertyChanged;

                _onBoardingTask = _onboardingViewModel.ConnectUser(_oauthCts.Token);
                ScheduleRestartButtonEnableAsync();
                if ((App.Current as App)?.CurrentWindow is OnBoardingWindow onBoardingWindow)
                    onBoardingWindow.UpdateLottieSource("Infomaniak.Custom.Animations.loader-stroke", 130);
            }
            else
            {
                Logger.Log(Logger.Level.Error,
                    "OnBoardingViewModel parameter missing when navigating to OAuthLoadingPage");
            }
        }

        protected override void OnNavigatedFrom(NavigationEventArgs e)
        {
            base.OnNavigatedFrom(e);
            if (_onboardingViewModel is not null)
            {
                _onboardingViewModel.PropertyChanged -= OnOnboardingViewModelPropertyChanged;
            }
            OnBoardingWindow? onBoardingWindow = (App.Current as App)?.CurrentWindow as OnBoardingWindow;
            if (onBoardingWindow is null)
            {
                Logger.Log(Logger.Level.Error, "Current window is not OnBoardingWindow - cannot reset Lottie position");
            }
            else
            {
                onBoardingWindow.SetLottiePosition(OnBoardingWindow.LottiePosition.Right);
            }

            _oauthCts.Cancel();
            _oauthCts.Dispose();

            _enableRestartCts.Cancel();
            _enableRestartCts.Dispose();
            Utility.VisualTreeDisposeUtility.DisposePageItems(this);
        }

        private async void OnOnboardingViewModelPropertyChanged(object? sender, PropertyChangedEventArgs e)
        {
            if (e.PropertyName == nameof(ViewModels.Onboarding.CurrentOAuth2State) &&
                _onboardingViewModel is not null)
            {
                await HandleOAuth2StateChanged(_onboardingViewModel.CurrentOAuth2State);
            }
        }

        private async Task HandleOAuth2StateChanged(OAuth2State state)
        {
            OnBoardingWindow? onBoardingWindow = (App.Current as App)?.CurrentWindow as OnBoardingWindow;

            if (onBoardingWindow is null)
            {
                Logger.Log(Logger.Level.Error, "Current window is not OnBoardingWindow - cannot update UI for OAuth2State change");
                return;
            }

            switch (state)
            {
                case OAuth2State.None:
                    // Should not happen
                    Logger.Log(Logger.Level.Warning, "OAuth2State is None - this should not happen");
                    break;

                case OAuth2State.WaitingForUserAction:
                    onBoardingWindow.SetLottiePosition(OnBoardingWindow.LottiePosition.Right);
                    TitleTextBlock.Text = Localizer.Instance.GetString("signInBrowser");
                    SubtitleTextBlock.Text = Localizer.Instance.GetString("browserSignInInstruction");
                    RestartOAuthButton.Visibility = Visibility.Visible;
                    break;

                case OAuth2State.ProcessingResponse:
                    onBoardingWindow.SetLottiePosition(OnBoardingWindow.LottiePosition.FullWindow);
                    TitleTextBlock.Text = Localizer.Instance.GetString("onboardingLoginProcessingTitle");
                    SubtitleTextBlock.Text = Localizer.Instance.GetString("onboardingLoginProcessingDescription");
                    RestartOAuthButton.Visibility = Visibility.Collapsed;
                    break;

                case OAuth2State.Success:
                    // OnBoardingWindow.LottiePosition will be set back to right when navigating from this page to avoid flickering
                    if (_onboardingViewModel?.SelectedUser is not null && await _onboardingViewModel.SelectedUser.RefreshAvailableDrives(CancellationToken.None))
                    {
                        if (_onboardingViewModel.SelectedUser.AllDrives.Any())
                        {
                            Frame.Navigate(typeof(DriveSelectionPage), _onboardingViewModel);
                        }
                        else
                        {
                            Frame.Navigate(typeof(NoDrivesPage), _onboardingViewModel);
                        }
                    }
                    else
                    {
                        Logger.Log(Logger.Level.Error, "SelectedUser is not set after a successful oAuth");
                        _onboardingViewModel!.CurrentOAuth2State = OAuth2State.Error;
                    }
                    break;

                case OAuth2State.Error:
                    _analyticsService.TrackPageView(Analytics.Keys.Category.OnboardingConnectionFailedPage);
                    onBoardingWindow.SetLottiePosition(OnBoardingWindow.LottiePosition.Right);
                    TitleTextBlock.Text = Localizer.Instance.GetString("onboardingLoginErrorTitle");
                    SubtitleTextBlock.Text = Localizer.Instance.GetString("onboardingLoginErrorDescription");
                    RestartOAuthButton.IsEnabled = true;
                    RestartOAuthButton.Visibility = Visibility.Visible;
                    await _enableRestartCts.CancelAsync();
                    break;
            }
        }

        private async void RestartOAuthButton_Click(object sender, RoutedEventArgs e)
        {
            _analyticsService.TrackClick(Analytics.Keys.Category.OnboardingConnectionFailedPage, Analytics.Keys.EventName.ReOpenLoginWeb);
            try
            {
                // Cancel any ongoing authentication attempt
                await _oauthCts.CancelAsync();
                if (_onBoardingTask is not null)
                {
                    await _onBoardingTask;
                }
            }
            catch (TaskCanceledException)
            {
                // Expected if cancellation was requested
            }
            finally
            {
                _oauthCts.Dispose();
                _oauthCts = new CancellationTokenSource(_oauthTimeOut);
                _onBoardingTask = _onboardingViewModel?.ConnectUser(_oauthCts.Token);
                ScheduleRestartButtonEnableAsync();
            }
        }

        private async Task TemporarilyDisableRestartButtonAsync()
        {
            RestartOAuthButton.IsEnabled = false;
            try
            {
                await _enableRestartCts.CancelAsync();
                if (_enableRestartTask is not null)
                {
                    await _enableRestartTask;

                }
            }
            catch (TaskCanceledException)
            {
                // Expected if cancellation was requested
            }
            finally
            {
                _enableRestartCts.Dispose();
            }
            _enableRestartCts = new CancellationTokenSource();
            var token = _enableRestartCts.Token;

            try
            {
                await Task.Delay(TimeSpan.FromSeconds(10), token);
                AppModel.UIThreadDispatcher.TryEnqueue(() =>
                {
                    RestartOAuthButton.IsEnabled = true;
                });
            }
            catch (TaskCanceledException)
            {
                // Expected if cancellation is requested
            }
        }

        private void ScheduleRestartButtonEnableAsync()
        {
            _enableRestartTask = TemporarilyDisableRestartButtonAsync();
        }
    }
}
