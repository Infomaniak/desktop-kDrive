using Infomaniak.kDrive.ViewModels;
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
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.UI;

namespace Infomaniak.kDrive.OnBoarding
{
    public sealed partial class WelcomePage : Page
    {
        private AppModel _viewModel = ((App)Application.Current).Data;
        private OnBoardingViewModel _onBoardingViewModel;
        public AppModel ViewModel { get { return _viewModel; } }
        public WelcomePage()
        {
            Logger.Log(Logger.Level.Info, "Navigated to WelcomePage - Initializing WelcomePage components");
            InitializeComponent();
            Logger.Log(Logger.Level.Debug, "WelcomePage components initialized");
        }
    }
}
