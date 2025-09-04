using ABI.System;
using KDrive.ViewModels;
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
using System.Windows.Input;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.UI;

namespace KDrive
{
    public sealed partial class ErrorPage : Page
    {
        public AppModel _viewModel = ((App)Application.Current).Data;
        public AppModel ViewModel { get { return _viewModel; } }
        public ErrorPage()
        {
            Logger.Log(Logger.Level.Info, "Navigated to ErrorPage - Initializing ErrorPage components");
            InitializeComponent();
            var resourceLoader = new Microsoft.Windows.ApplicationModel.Resources.ResourceLoader();
            NavBar.ItemsSource = new string[] { resourceLoader.GetString("NavActivity/Content"), resourceLoader.GetString("ErrorPage_Title/Text") };
            Logger.Log(Logger.Level.Debug, "ErrorPage components initialized");
        }

        private void NavBar_ItemClicked(BreadcrumbBar sender, BreadcrumbBarItemClickedEventArgs args)
        {
            if (args.Index == 0)
            {
                Logger.Log(Logger.Level.Debug, "Navigating to ActivityPage from ErrorPage");
                Frame.Navigate(typeof(ActivityPage));
            }
        }

        private async void SupportButton_Click(object sender, RoutedEventArgs e)
        {
            Logger.Log(Logger.Level.Info, "Support button clicked, opening support URL");
            var resourceLoader = Windows.ApplicationModel.Resources.ResourceLoader.GetForViewIndependentUse();
            await Windows.System.Launcher.LaunchUriAsync(new System.Uri(resourceLoader.GetString("Global_SupportUrl")));
            Logger.Log(Logger.Level.Debug, "Support URL opened");
        }
    }
}
