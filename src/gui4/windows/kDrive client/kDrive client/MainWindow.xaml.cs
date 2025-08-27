using kDrive_client.DataModel;
using kDrive_client.ServerCommunication;
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
using System.Threading.Tasks;
using Windows.Foundation;
using Windows.Foundation.Collections;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace kDrive_client
{
    public sealed partial class MainWindow : Window
    {
        private DriveSelector _driveSelector = new DriveSelector();
        private DataModel.AppModel _dataModel => ((App)Application.Current).Data;
        public MainWindow()
        {
            InitializeComponent();
            this.ExtendsContentIntoTitleBar = true;  // enable custom titlebar
            this.SetTitleBar(AppTitleBar);
            _driveSelector.DriveSelectorFlyout = DriveSelection;

            _ = Task.Run(async () =>
            {
                Random rand = new Random();
                int counter = 50;
                while (true)
                {
                    await Task.Delay(1000);
                    if (_dataModel.Drives.Count > 0)
                    {
                        var drive = _dataModel.Drives.First();
                        DispatcherQueue.TryEnqueue(() =>
                        {
                            drive.Name = "Drive_" + rand.Next(1000, 9999).ToString();
                        });
                    }
                    DispatcherQueue.TryEnqueue(() =>
                    {
                        _dataModel.Drives.Add(new Drive(++counter) { Name = "Drive_" + rand.Next(1000, 9999).ToString() });
                    });
                }
            });
        }

        internal DataModel.AppModel ViewModel { get => _dataModel; }

        private void nvSample_SelectionChanged(NavigationView sender, NavigationViewSelectionChangedEventArgs args)
        {
            var selectedItem = args.SelectedItem as NavigationViewItem;
            if (selectedItem != null)
            {
                // Navigate to the selected page
                switch (selectedItem.Tag)
                {
                    default:
                        contentFrame.Navigate(typeof(HomePage));
                        break;
                }
            }
        }
    }

    public class DriveSelector
    {
        public MenuFlyout? DriveSelectorFlyout { get; set; }
        internal Drive? SelectedDrive { get; set; } = null;
    }

}
