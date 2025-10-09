using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
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

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class SyncErrorBar : UserControl
    {
        private AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        public AppModel ViewModel => _viewModel;

        public SyncErrorBar()
        {
            InitializeComponent();
        }

        private void CheckFileButton_Click(object sender, RoutedEventArgs e)
        {
            // Go up the visual tree to find the Frame
            Logger.Log(Logger.Level.Debug, "Navigating to ErrorPage for error resolution.");
            DependencyObject parent = this;
            while (parent != null && parent is not Frame)
            {
                parent = VisualTreeHelper.GetParent(parent);
            }
            if (parent is Frame frame)
            {
                Logger.Log(Logger.Level.Info, "Navigating to ErrorPage.");
                frame.Navigate(typeof(Pages.ErrorPage));
            }
            else
            {
                Logger.Log(Logger.Level.Error, "Could not find Frame in visual tree to navigate to error resolution page");
            }

        }
    }
}
