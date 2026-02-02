using Infomaniak.kDrive.OnBoarding;
using Infomaniak.kDrive.ViewModels;
using Infomaniak.kDrive.Types;
using Microsoft.UI;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.ComponentModel;
using System.Threading;
using System.Threading.Tasks;
using System.Linq;
using Microsoft.Extensions.DependencyInjection;


namespace Infomaniak.kDrive.Pages.Onboarding
{
    public sealed partial class OAuthLoadingPage : Page
    {
        private static TimeSpan _oauthTimeOut = TimeSpan.FromMinutes(5);
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

            _oauthCts.Cancel();
            _oauthCts.Dispose();

            _enableRestartCts.Cancel();
            _enableRestartCts.Dispose();
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
            switch (state)
            {
                case OAuth2State.None:
                    // Should not happen
                    Logger.Log(Logger.Level.Warning, "OAuth2State is None - this should not happen");
                    break;

                case OAuth2State.WaitingForUserAction:
                    TitleTextBlock.Text = Utility.GetLocalizedString("Page_Onboarding_OAuthLoadingPage_ConnectInNavigator_Title/Text");
                    SubtitleTextBlock.Text = Utility.GetLocalizedString("Page_Onboarding_OAuthLoadingPage_ConnectInNavigator_Subtitle/Text");
                    RestartOAuthButton.Visibility = Visibility.Visible;
                    break;

                case OAuth2State.ProcessingResponse:
                    TitleTextBlock.Text = Utility.GetLocalizedString("Page_Onboarding_OAuthLoadingPage_Processing_Title/Text");
                    SubtitleTextBlock.Text = Utility.GetLocalizedString("Page_Onboarding_OAuthLoadingPage_Processing_Subtitle/Text");
                    RestartOAuthButton.Visibility = Visibility.Collapsed;
                    break;

                case OAuth2State.Success:
                    TitleTextBlock.Text = Utility.GetLocalizedString("Page_Onboarding_OAuthLoadingPage_Success_Title/Text");
                    SubtitleTextBlock.Text = Utility.GetLocalizedString("Page_Onboarding_OAuthLoadingPage_Success_Subtitle/Text");
                    RestartOAuthButton.Visibility = Visibility.Collapsed;
                    if (_onboardingViewModel?.SelectedUser is not null)
                    {
                        await _onboardingViewModel.SelectedUser.RefreshAvailableDrives();
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
                        HandleOAuth2StateChanged(OAuth2State.Error);
                    }
                    break;

                case OAuth2State.Error:
                    TitleTextBlock.Text = Utility.GetLocalizedString("Page_Onboarding_OAuthLoadingPage_Error_Title/Text");
                    SubtitleTextBlock.Text = Utility.GetLocalizedString("Page_Onboarding_OAuthLoadingPage_Error_Subtitle/Text");
                    RestartOAuthButton.IsEnabled = true;
                    RestartOAuthButton.Visibility = Visibility.Visible;
                    await _enableRestartCts.CancelAsync();
                    break;
            }
        }

        private async void RestartOAuthButton_Click(object sender, RoutedEventArgs e)
        {
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
