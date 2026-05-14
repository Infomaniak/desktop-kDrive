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
using Infomaniak.kDrive.Pages.Errors;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Dispatching;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.Pages.Onboarding
{
    public sealed partial class NoDrivesPage : Page
    {
        private readonly IAnalyticsService _analyticsService = App.ServiceProvider.GetRequiredService<IAnalyticsService>();
        private readonly AppModel _appModel = App.ServiceProvider.GetRequiredService<AppModel>();
        private ViewModels.Onboarding? _onboardingViewModel;

        public AppModel AppModel => _appModel;
        public ViewModels.Onboarding? OnboardingViewModel => _onboardingViewModel;
        public NoDrivesPage()
        {
            Logger.Log(Logger.Level.Info, "Navigated to NoDrivesPage - Initializing components");
            InitializeComponent();
            Logger.Log(Logger.Level.Debug, "NoDrivesPage components initialized");
        }

        protected override void OnNavigatedTo(NavigationEventArgs e)
        {
            if (e.Parameter is ViewModels.Onboarding onboardingVm)
            {
                _onboardingViewModel = onboardingVm;
                if ((App.Current as App)?.CurrentWindow is OnBoardingWindow onBoardingWindow)
                    onBoardingWindow.UpdateLottieSource("Infomaniak.Custom.Animations.synchro-file", 219);

                onboardingVm.DrivesAvailable += OnDrivesAvailable;
                onboardingVm.StartDriveAvailabilityWatcher();
            }
            else
            {
                Logger.Log(Logger.Level.Fatal, "OnboardingViewModel parameter missing when navigating to NoDrivesPage");
                throw new Exception("OnboardingViewModel parameter missing when navigating to NoDrivesPage");
            }
        }

        protected override void OnNavigatedFrom(NavigationEventArgs e)
        {
            DetachEventHandlers();  
            Utility.VisualTreeDisposeUtility.DisposePageItems(this);

        }

        private void DetachEventHandlers()
        {
            if (_onboardingViewModel is not null)
            {
                _onboardingViewModel.DrivesAvailable -= OnDrivesAvailable;
                _ = _onboardingViewModel.StopDriveAvailabilityWatcherAsync();
            }
        }

        private void OnDrivesAvailable(object? sender, EventArgs e)
        {
            DispatcherQueue.TryEnqueue(DispatcherQueuePriority.Normal, () =>
            {
                if (_onboardingViewModel is not null)
                {
                    Logger.Log(Logger.Level.Info, "Drives found for user - Navigating to DriveSelectionPage");
                    Frame.Navigate(typeof(DriveSelectionPage), _onboardingViewModel);
                }
            });
        }

        private async void OnStartFreeButtonClick(object sender, RoutedEventArgs e)
        {
            _analyticsService.TrackClick(Analytics.Keys.Category.OnboardingSyncConfigurationPage, Analytics.Keys.EventName.OpenStartFreeWeb);
            await Windows.System.Launcher.LaunchUriAsync(App.Constants.Drive.StartFreeUri);
        }

        private async void OnOffersButtonClick(object sender, RoutedEventArgs e)
        {
            _analyticsService.TrackClick(Analytics.Keys.Category.OnboardingSyncConfigurationPage, Analytics.Keys.EventName.OpenOffersWeb);
            await Windows.System.Launcher.LaunchUriAsync(App.Constants.kSuite.TarrifsUri);
        }

        private void ChangeUserButton_Click(object sender, RoutedEventArgs e)
        {
            OnboardingViewModel?.Reset();
            Frame.Navigate(typeof(Onboarding.WelcomePage), OnboardingViewModel);
        }
    }
}
