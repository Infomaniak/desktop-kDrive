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

        protected override async void OnNavigatedTo(NavigationEventArgs e)
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

        protected override async void OnNavigatedFrom(NavigationEventArgs e)
        {
            await DetachEventHandlers();  
            Bindings.StopTracking();
        }

        private async void Page_Unloaded(object sender, RoutedEventArgs e)
        {
            await DetachEventHandlers();
        }

        private async Task DetachEventHandlers()
        {
            if (_onboardingViewModel is not null)
            {
                _onboardingViewModel.DrivesAvailable -= OnDrivesAvailable;
                await _onboardingViewModel.StopDriveAvailabilityWatcherAsync();
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
            await Windows.System.Launcher.LaunchUriAsync(App.Constants.Drive.StartFreeUri);
        }

        private async void OnOffersButtonClick(object sender, RoutedEventArgs e)
        {
            await Windows.System.Launcher.LaunchUriAsync(App.Constants.kSuite.TarrifsUri);
        }

        private void ChangeUserButton_Click(object sender, RoutedEventArgs e)
        {
            OnboardingViewModel?.Reset();
            Frame.Navigate(typeof(Onboarding.WelcomePage), OnboardingViewModel);
        }
    }
}
