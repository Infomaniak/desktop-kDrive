using H.NotifyIcon;
using kDrive_client.ServerCommunication;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Data;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Media.Imaging;
using Microsoft.UI.Xaml.Navigation;
using Microsoft.UI.Xaml.Shapes;
using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Threading.Tasks;
using Windows.ApplicationModel;
using Windows.ApplicationModel.Activation;
using Windows.Foundation;
using Windows.Foundation.Collections;

namespace kDrive_client
{
    public partial class App : Application
    {
        public Window? Window { get; set; }
        public TrayIcon.TrayIconManager TrayIcoManager { get; private set; }

        internal ServerCommunication.CommClient ComClient { get; set; } = new ServerCommunication.CommClient();
        internal DataModel.AppModel Data { get; set; } = new DataModel.AppModel();
        public App()
        {
            InitializeComponent();
            TrayIcoManager = new TrayIcon.TrayIconManager();
        }

        protected override async void OnLaunched(Microsoft.UI.Xaml.LaunchActivatedEventArgs args)
        {
            TrayIcoManager.Initialize(Window);
            await Data.InitializeAsync().ConfigureAwait(false);
        }
    }
}
