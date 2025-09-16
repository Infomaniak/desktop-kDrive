/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

using H.NotifyIcon;
using Infomaniak.kDrive.ServerCommunication;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Security.Authentication.OAuth;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Data;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Media.Imaging;
using Microsoft.UI.Xaml.Navigation;
using Microsoft.UI.Xaml.Shapes;
using Microsoft.VisualBasic.Logging;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Threading.Tasks;
using Windows.ApplicationModel;
using Windows.ApplicationModel.Activation;
using Windows.Foundation;
using Windows.Foundation.Collections;

namespace Infomaniak.kDrive
{
    public partial class App : Application
    {
        private Window? _currentWindow;
        public Window? CurrentWindow
        {
            get => _currentWindow;
            private set
            {
                _currentWindow = value;
                TrayIcoManager.ConfigureWindowEventHandler();
            }
        }
        public TrayIcon.TrayIconManager TrayIcoManager { get; private set; }
        public ServerCommunication.CommClient ComClient { get; set; } = new ServerCommunication.CommClient();
        public AppModel Data { get; set; } = new AppModel();

        public App()
        {
            InitializeComponent();
            TrayIcoManager = new TrayIcon.TrayIconManager();
            Logger.Log(Logger.Level.Info, "Application started");
        }

        protected override async void OnLaunched(Microsoft.UI.Xaml.LaunchActivatedEventArgs args)
        {
            string[] arguments = Environment.GetCommandLineArgs();
            if (arguments.Length > 1)
            {
                var oauthArg = arguments.FirstOrDefault(arg => arg.StartsWith("kdrive://auth-desktop"), "");
                if (oauthArg != "")
                {
                    if (OAuth2Manager.CompleteAuthRequest(new Uri(oauthArg)))
                    {
                        // Terminate the Process
                        Logger.Log(Logger.Level.Info, "OAuth process completed, response routed successfully. Terminating the process.");
                    }
                    else
                    {
                        Logger.Log(Logger.Level.Warning, "OAuth process failed.");
                    }
                    Process current = Process.GetCurrentProcess();
                    current.Kill();
                }
            }

            Logger.Log(Logger.Level.Info, $"App launched with kind: {args.UWPLaunchActivatedEventArgs.Kind}, arguments: {args.Arguments}");
            AppModel.UIThreadDispatcher = Microsoft.UI.Dispatching.DispatcherQueue.GetForCurrentThread(); // Save the UI thread dispatcher for later use in view models
            CurrentWindow = new MainWindow();
            TrayIcoManager.Initialize();
            await ComClient.Initialize();
            await Data.InitializeAsync().ConfigureAwait(false);
        }
   
        public void StartOnBoarding()
        {
            CurrentWindow?.Close();
            CurrentWindow = new OnBoarding.OnBoardingWindow();
            ((OnBoarding.OnBoardingWindow)CurrentWindow).Closed += (s, e) =>
            {
                Logger.Log(Logger.Level.Info, "OnBoardingWindow closed, restarting MainWindow.");
                CurrentWindow = new MainWindow();
                CurrentWindow.Activate();
            };

            CurrentWindow.Activate();
        }
    }
}
