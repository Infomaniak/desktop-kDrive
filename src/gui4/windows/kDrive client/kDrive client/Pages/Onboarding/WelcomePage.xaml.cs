using Infomaniak.kDrive.Pages.Onboarding;
using Infomaniak.kDrive.ViewModels;
using Infomaniak.kDrive.OnBoarding;
using Microsoft.UI;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Data;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Threading.Tasks;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.UI;
using Microsoft.Extensions.DependencyInjection;

namespace Infomaniak.kDrive.Pages.Onboarding
{
    public sealed partial class WelcomePage : Page
    {
        private AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
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
            }
        }

        private async void SignupButton_Click(object sender, RoutedEventArgs e)
        {
            Logger.Log(Logger.Level.Info, "Create account button clicked, opening sign up URL");
            if (sender is Button btn)
            {
                btn.IsEnabled = false;

                var resourceLoader = Windows.ApplicationModel.Resources.ResourceLoader.GetForViewIndependentUse();
                await Windows.System.Launcher.LaunchUriAsync(new System.Uri(resourceLoader.GetString("Global_SignUpUrl")));
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
