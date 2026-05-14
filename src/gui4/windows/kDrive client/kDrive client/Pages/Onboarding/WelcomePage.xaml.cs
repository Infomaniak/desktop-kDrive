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
        private readonly IAnalyticsService _analyticsService = App.ServiceProvider.GetRequiredService<IAnalyticsService>();
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
            _analyticsService.TrackPageView(Analytics.Keys.Category.OnboardingWelcomePage);
            if (e.Parameter is ViewModels.Onboarding obvm)
            {
                _onBoardingViewModel = obvm;
                if ((App.Current as App)?.CurrentWindow is OnBoardingWindow onBoardingWindow)
                    onBoardingWindow.UpdateLottieSource("Infomaniak.Custom.Animations.loader-stroke", 130, 0);
            }
        }
        protected override void OnNavigatedFrom(NavigationEventArgs e)
        {
            Utility.VisualTreeDisposeUtility.DisposePageItems(this);
        }
        private async void SignupButton_Click(object sender, RoutedEventArgs e)
        {
            Logger.Log(Logger.Level.Info, "Create account button clicked, opening sign up URL");
            _analyticsService.TrackClick(Analytics.Keys.Category.OnboardingWelcomePage, Analytics.Keys.EventName.OpenSignUpWeb);
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
            _analyticsService.TrackClick(Analytics.Keys.Category.OnboardingWelcomePage, Analytics.Keys.EventName.OpenSignInWeb);
            if (sender is Button btn)
            {
                btn.IsEnabled = false;
                Frame.Navigate(typeof(OAuthLoadingPage), _onBoardingViewModel);
                btn.IsEnabled = true;
            }
        }
    }
}
