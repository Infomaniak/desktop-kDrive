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

namespace Infomaniak.kDrive.Pages.Onboarding
{
    public sealed partial class FinishingPage : Page
    {
        private AppModel _viewModel = ((App)Application.Current).Data;
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
                await _onBoardingViewModel.FinishOnboarding();
                Frame.Navigate(typeof(FinishPage), _onBoardingViewModel);
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
