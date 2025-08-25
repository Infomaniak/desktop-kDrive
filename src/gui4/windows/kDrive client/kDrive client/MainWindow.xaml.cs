using kDrive_client.Model;
using Microsoft.UI.Input;
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

namespace kDrive_client
{
    public sealed partial class MainWindow : Window
    {
        private DriveSelector DriveSelector = new DriveSelector();
        public MainWindow()
        {
            InitializeComponent();
            this.ExtendsContentIntoTitleBar = true;  // enable custom titlebar
            this.SetTitleBar(AppTitleBar);
            DriveSelector.DriveSelectorFlyout = DriveSelection;
        }
        static int count = 0;

        private void nvSample_SelectionChanged(NavigationView sender, NavigationViewSelectionChangedEventArgs args)
        {
            var selectedItem = args.SelectedItem as NavigationViewItem;
            if (selectedItem != null)
            {
                // Navigate to the selected page
                switch (selectedItem.Tag)
                {
                    case "Home":
                        contentFrame.Navigate(typeof(HomePage));
                        break;
                    default:
                        contentFrame.Navigate(typeof(HomePage));
                        break;
                }
                DriveSelector.SelectedDrive.Name = "Drive A " + count++;
            }
        }

        private void AutoSuggestBox_QuerySubmitted(AutoSuggestBox sender, AutoSuggestBoxQuerySubmittedEventArgs args)
        {

        }
    }

    public class DriveSelector
    {
        public MenuFlyout? DriveSelectorFlyout { get; set; }
        internal Drive SelectedDrive { get; set; } = new Drive();

    }

}
