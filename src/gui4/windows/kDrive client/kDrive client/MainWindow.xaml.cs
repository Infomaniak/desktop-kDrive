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
        internal DataModel.AppModel ViewModel => ((App)Application.Current).Data;
        public MainWindow()
        {
            InitializeComponent();
            DataModel.AppModel.UIThreadDispatcher = Microsoft.UI.Dispatching.DispatcherQueue.GetForCurrentThread(); ;
            this.ExtendsContentIntoTitleBar = true;  // enable custom titlebar
            this.SetTitleBar(AppTitleBar);


            // For testing Purpose only: Simulate live changes in the data model to check if UI updates correctly
            _ = Task.Run(async () => // Task.Run will run this code in a background thread != UI thread
           {
               Random rand = new Random();
               int counter = 50;
               while (true)
               {
                   await Task.Delay(1000);
                   User user;
                   // Choose a random user
                   if (ViewModel.Users.Count > 0)
                   {
                       user = ViewModel.Users[rand.Next(0, ViewModel.Users.Count)];
                       // Add a new drive to this user
                       Drive drive = new Drive(counter++);
                       drive.Name = "Drive " + counter;
                       DispatcherQueue.TryEnqueue(() =>
                       {
                           // Those action must be done on the UI thread because they modify observable collections bound to the UI
                           user.Drives.Add(drive);
                           user.Drives[0].Name = "Changed name " + rand.Next(0, 100);
                       });

                   }
               }
           });
        }

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
}
