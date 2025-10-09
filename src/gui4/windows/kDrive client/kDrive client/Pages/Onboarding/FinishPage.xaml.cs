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
    public sealed partial class FinishPage : Page
    {
        private AppModel _viewModel = (App.Current as App).Data;
        private ViewModels.Onboarding? _onBoardingViewModel;
        public AppModel ViewModel { get { return _viewModel; } }
        public FinishPage()
        {
            Logger.Log(Logger.Level.Info, "Navigated to FinishPage - Initializing FinishPage components");
            InitializeComponent();
            Logger.Log(Logger.Level.Debug, "FinishPage components initialized");
        }

        protected override void OnNavigatedTo(NavigationEventArgs e)
        {
            if (e.Parameter is ViewModels.Onboarding obvm)
            {
                _onBoardingViewModel = obvm;
            }
        }

        private void FinishButton_Click(object sender, RoutedEventArgs e)
        {
            // Close this window
            (Application.Current as App)?.CurrentWindow?.Close();
        }
    }
}
