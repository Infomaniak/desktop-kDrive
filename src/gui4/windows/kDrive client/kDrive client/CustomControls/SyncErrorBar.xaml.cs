using KDrive.ViewModels;
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

namespace KDrive.CustomControls
{
    public sealed partial class SyncErrorBar : UserControl
    {
        internal AppModel _viewModel = ((App)Application.Current).Data;
        internal AppModel ViewModel
        {
            get { return _viewModel; }
        }

        public SyncErrorBar()
        {
            InitializeComponent();
        }

        private void CheckFileButton_Click(object sender, RoutedEventArgs e)
        {
           if(sender is DependencyObject obj)
            {
                // Go up the visual tree to find the Frame
                DependencyObject parent = obj;
                while (parent != null && parent is not Frame)
                {
                    parent = VisualTreeHelper.GetParent(parent);
                }
                if (parent is Frame frame)
                {
                    frame.Navigate(typeof(HomePage));
                }
            }
        }
    }
}
