using Infomaniak.kDrive.OnBoarding;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using System;

namespace Infomaniak.kDrive.Pages.Onboarding
{
    public sealed partial class FinishPage : Page
    {
        private readonly AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        public AppModel ViewModel { get { return _viewModel; } }
        public FinishPage()
        {
            Logger.Log(Logger.Level.Info, "Navigated to FinishPage - Initializing FinishPage components");
            InitializeComponent();
            Logger.Log(Logger.Level.Debug, "FinishPage components initialized");
        }
        protected override void OnNavigatedTo(NavigationEventArgs e)
        {
            if ((App.Current as App)?.CurrentWindow is OnBoardingWindow onBoardingWindow)
                onBoardingWindow.UpdateLottieSource("Infomaniak.Custom.Animations.loader-stroke", 130, 1);
        }

        private void FinishButton_Click(object sender, RoutedEventArgs e)
        {
            // Close this window
            (Application.Current as App)?.CurrentWindow?.Close();
        }
    }
}
