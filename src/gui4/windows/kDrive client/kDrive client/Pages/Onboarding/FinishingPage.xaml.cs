using Infomaniak.kDrive.OnBoarding;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using System;

namespace Infomaniak.kDrive.Pages.Onboarding
{
    public sealed partial class FinishingPage : Page
    {
        private AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        private ViewModels.Onboarding? _onBoardingViewModel;
        public AppModel ViewModel { get { return _viewModel; } }

        public FinishingPage()
        {
            Logger.Log(Logger.Level.Info, "Navigated to FinishingPage - Initializing FinishPage components");
            InitializeComponent();
            Logger.Log(Logger.Level.Debug, "FinishingPage components initialized");
        }

        protected async override void OnNavigatedTo(NavigationEventArgs e)
        {
            if (e.Parameter is ViewModels.Onboarding obvm)
            {
                _onBoardingViewModel = obvm;

                if (_onBoardingViewModel.NewSyncs.Count == 0)
                {
                    Logger.Log(Logger.Level.Warning, "No drives selected to sync. User must select at least one drive.");
                    Frame.GoBack();
                    return;
                }
                if ((App.Current as App)?.CurrentWindow is OnBoardingWindow onBoardingWindow)
                    onBoardingWindow.UpdateLottieSource("Infomaniak.Custom.Animations.synchro-file", 219);

                if (await _onBoardingViewModel.FinishOnboarding())
                    Frame.Navigate(typeof(FinishPage), _onBoardingViewModel);
                else
                {
                    Logger.Log(Logger.Level.Info, "Finishing onboarding failed. Returning to previous page.");
                    Frame.GoBack();
                }
            }
            else
            {
                var errorMessage = "OnBoardingViewModel parameter missing when navigating to FinishingPage";
                Logger.Log(Logger.Level.Fatal, errorMessage);
                throw new Exception(errorMessage);
            }
        }
    }
}
