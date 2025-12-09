using Infomaniak.kDrive.OnBoarding;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.Linq;
using System.Threading.Tasks;
using System.Threading;

namespace Infomaniak.kDrive.Pages.Onboarding
{
    public sealed partial class NoDrivesPage : Page
    {
        private readonly AppModel _appModel = App.ServiceProvider.GetRequiredService<AppModel>();
        private ViewModels.Onboarding? _onboardingViewModel;
        public AppModel AppModel => _appModel;
        public ViewModels.Onboarding? OnboardingViewModel => _onboardingViewModel;
        private Task _driveAvailableWatcherTask;
        private CancellationTokenSource _driveAvailableWatcherCts = new();
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

                Logger.Log(Logger.Level.Info, "Starting drive availability watcher task");
                _driveAvailableWatcherCts = new CancellationTokenSource();
                _driveAvailableWatcherTask = WatchAvailableDrives(_driveAvailableWatcherCts.Token);
            }
            else
            {
                Logger.Log(Logger.Level.Fatal, "OnboardingViewModel parameter missing when navigating to NoDrivesPage");
                throw new Exception("OnboardingViewModel parameter missing when navigating to NoDrivesPage");
            }
        }

        protected override async void OnNavigatedFrom(NavigationEventArgs e)
        {
            if (_driveAvailableWatcherTask is not null)
            {
                Logger.Log(Logger.Level.Info, "Cancelling drive availability watcher task");
                _driveAvailableWatcherCts.Cancel();
                try
                {
                    await _driveAvailableWatcherTask;
                }
                catch (OperationCanceledException)
                {
                    Logger.Log(Logger.Level.Info, "Drive availability watcher task cancelled successfully");
                }
            }
        }

        private async Task WatchAvailableDrives(CancellationToken cancellationToken)
        {
            try
            {
                while (!cancellationToken.IsCancellationRequested)
                {
                    await CheckAvailableDrives(cancellationToken);
                    await Task.Delay(5000, cancellationToken); // Check every 5 seconds
                }

            }
            catch (OperationCanceledException)
            {
                Logger.Log(Logger.Level.Info, "Drive availability watcher task cancelled");
            }

        }

        private async Task CheckAvailableDrives(CancellationToken cancellationToken)
        {
            await OnboardingViewModel.SelectedUser.RefreshAvailableDrives(cancellationToken);
            if (!cancellationToken.IsCancellationRequested && OnboardingViewModel.SelectedUser.AllDrives.Any())
            {
                Logger.Log(Logger.Level.Info, "Drives found for user - Navigating to DriveSelectionPage");
                Frame.Navigate(typeof(DriveSelectionPage), OnboardingViewModel);
            }
        }


        private async void OnStartFreeButtonClick(object sender, RoutedEventArgs e)
        {
            await Windows.System.Launcher.LaunchUriAsync(new Uri(Utility.GetLocalizedString("Global_kDriveShopUrl")));
        }

        private async void OnOffersButtonClick(object sender, RoutedEventArgs e)
        {
            await Windows.System.Launcher.LaunchUriAsync(new Uri(Utility.GetLocalizedString("Global_kDriveOffersUrl")));
        }
    }
}
