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
        internal AppModel _viewModel = ((App)Application.Current).Data;
        internal AppModel ViewModel { get { return _viewModel; } }
        public ErrorPage()
        {
            InitializeComponent();
            var resourceLoader = new Microsoft.Windows.ApplicationModel.Resources.ResourceLoader();
            NavBar.ItemsSource = new string[] { resourceLoader.GetString("NavActivity/Content"), resourceLoader.GetString("ErrorPage_Title/Text") };
        }

        private void NavBar_ItemClicked(BreadcrumbBar sender, BreadcrumbBarItemClickedEventArgs args)
        {
            if (args.Index == 0)
            {
                Frame.Navigate(typeof(ActivityPage));
            }
        }
        private async void SupportButton_Click(object sender, RoutedEventArgs e)
        {
            var resourceLoader = Windows.ApplicationModel.Resources.ResourceLoader.GetForViewIndependentUse();
            await Windows.System.Launcher.LaunchUriAsync(new System.Uri(resourceLoader.GetString("Global_SupportUrl")));
        }
    }
}
