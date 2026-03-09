using Infomaniak.kDrive.OnBoarding;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.Pages.Onboarding
{
    public sealed partial class WelcomePage : Page
    {
        private readonly AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        private ViewModels.Onboarding? _onBoardingViewModel;
        public AppModel ViewModel { get { return _viewModel; } }
        public WelcomePage()
        {
            Logger.Log(Logger.Level.Info, "Navigated to WelcomePage - Initializing WelcomePage components");
            InitializeComponent();
            Logger.Log(Logger.Level.Debug, "WelcomePage components initialized");
        }

        protected override void OnNavigatedTo(NavigationEventArgs e)
        {
            if (e.Parameter is ViewModels.Onboarding obvm)
            {
                _onBoardingViewModel = obvm;
                if ((App.Current as App)?.CurrentWindow is OnBoardingWindow onBoardingWindow)
                    onBoardingWindow.UpdateLottieSource("Infomaniak.Custom.Animations.loader-stroke", 130, 1);
            }
        }

        private async void SignupButton_Click(object sender, RoutedEventArgs e)
        {
            Logger.Log(Logger.Level.Info, "Create account button clicked, opening sign up URL");
            if (sender is Button btn)
            {
                btn.IsEnabled = false;

                await Windows.System.Launcher.LaunchUriAsync(App.Constants.kSuite.HomeUri);
                Logger.Log(Logger.Level.Debug, "Create account URL opened");

                await Task.Delay(2000);
                btn.IsEnabled = true;
            }
        }
        private void SigninButton_Click(object sender, RoutedEventArgs e)
        {
            Logger.Log(Logger.Level.Info, "Sign in button clicked, starting authentication process");
            if (sender is Button btn)
            {
                btn.IsEnabled = false;
                Frame.Navigate(typeof(OAuthLoadingPage), _onBoardingViewModel);
                btn.IsEnabled = true;
            }
        }
    }
}
