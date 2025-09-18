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


namespace Infomaniak.kDrive.Pages.Onboarding
{
    public sealed partial class OAuthLoadingPage : Page, IDisposable
    {
        private static TimeSpan _oauthTimeOut = TimeSpan.FromMinutes(5);
        private readonly AppModel _viewModel = ((App)Application.Current).Data;
        private ViewModels.Onboarding? _onboardingViewModel;

        private Task? _onBoardingTask;
        private CancellationTokenSource _onAuthCts = new(_oauthTimeOut);

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

                _onBoardingTask = _onboardingViewModel.ConnectUser(_onAuthCts.Token);
                ScheduleRestartButtonEnableAsync();
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
            Dispose();
        }

        private void OnOnboardingViewModelPropertyChanged(object? sender, PropertyChangedEventArgs e)
        {
            if (e.PropertyName == nameof(ViewModels.Onboarding.CurrentOAuth2State) &&
                _onboardingViewModel is not null)
            {
                HandleOAuth2StateChanged(_onboardingViewModel.CurrentOAuth2State);
            }
        }

        private async void HandleOAuth2StateChanged(OAuth2State state)
        {
            var ressourceLoader = Windows.ApplicationModel.Resources.ResourceLoader.GetForViewIndependentUse();
            switch (state)
            {
                case OAuth2State.None:
                    // Should not happen
                    break;

                case OAuth2State.WaitingForUserAction:
                    LoadingProgressBar.ShowError = false;
                    LoadingProgressBar.IsIndeterminate = true;
                    LoadingProgressBar.Foreground = new SolidColorBrush(Colors.Blue);
                    TitleTextBlock.Text = ressourceLoader.GetString("Page_Onboarding_OAuthLoadingPage_ConnectInNavigator_Title/Text");
                    SubtitleTextBlock.Text = ressourceLoader.GetString("Page_Onboarding_OAuthLoadingPage_ConnectInNavigator_Subtitle/Text");
                    RestartOAuthButton.Visibility = Visibility.Visible;
                    break;

                case OAuth2State.ProcessingResponse:
                    LoadingProgressBar.ShowError = false;
                    LoadingProgressBar.IsIndeterminate = true;
                    LoadingProgressBar.Foreground = new SolidColorBrush(Colors.Blue);
                    TitleTextBlock.Text = ressourceLoader.GetString("Page_Onboarding_OAuthLoadingPage_Processing_Title/Text");
                    SubtitleTextBlock.Text = ressourceLoader.GetString("Page_Onboarding_OAuthLoadingPage_Processing_Subtitle/Text");
                    RestartOAuthButton.Visibility = Visibility.Collapsed;
                    break;

                case OAuth2State.Success:
                    LoadingProgressBar.ShowError = false;
                    LoadingProgressBar.Value = 100;
                    LoadingProgressBar.IsIndeterminate = false;
                    LoadingProgressBar.Foreground = new SolidColorBrush(Colors.LimeGreen);
                    TitleTextBlock.Text = ressourceLoader.GetString("Page_Onboarding_OAuthLoadingPage_Success_Title/Text");
                    SubtitleTextBlock.Text = ressourceLoader.GetString("Page_Onboarding_OAuthLoadingPage_Success_Subtitle/Text");
                    RestartOAuthButton.Visibility = Visibility.Collapsed;

                    await Task.Delay(1000);
                    AppModel.UIThreadDispatcher.TryEnqueue(() =>
                    {
                        Frame.Navigate(typeof(DriveSelectionPage), _onboardingViewModel);
                    });
                    break;

                case OAuth2State.Error:
                    LoadingProgressBar.ShowError = true;
                    LoadingProgressBar.IsIndeterminate = false;
                    TitleTextBlock.Text = "Erreur de connexion";
                    SubtitleTextBlock.Text = "Une erreur est survenue lors de la connexion. Veuillez réessayer.";
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
                await _onAuthCts.CancelAsync();
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
                _onAuthCts.Dispose();
                _onAuthCts = new CancellationTokenSource(_oauthTimeOut); // OAuth attempts should timeout after 20 seconds
                _onBoardingTask = _onboardingViewModel?.ConnectUser(_onAuthCts.Token);
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

        public void Dispose()
        {
            if (_onboardingViewModel is not null)
            {
                _onboardingViewModel.PropertyChanged -= OnOnboardingViewModelPropertyChanged;
            }

            _onAuthCts.Cancel();
            _onAuthCts.Dispose();

            _enableRestartCts.Cancel();
            _enableRestartCts.Dispose();
        }
    }
}
