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

using Infomaniak.kDrive.CustomControls;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Input;
using System;
using System.IO;
using System.Linq;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Infomaniak.kDrive
{
    public sealed partial class MainWindow : Window
    {
        public AppNavigationView AppNavView { get { return NavView; } }
        public MainWindow()
        {
            InitializeComponent();
            this.ExtendsContentIntoTitleBar = true;  // enable custom titlebar
            this.SetTitleBar(AppTitleBar);
            Utility.SetWindowProperties(this, 900, 530, true);
            AppModel.UIThreadDispatcher = Microsoft.UI.Dispatching.DispatcherQueue.GetForCurrentThread(); // Save the UI thread dispatcher for later use in view models
        }

        private void AppTitleBarLogo_PointerPressed(object sender, PointerRoutedEventArgs e)
        {
            int legacyCommPort = ((App)Application.Current).LegacyCommPort;


            try
            {
                if (legacyCommPort <= 0)
                {
                    Logger.Log(Logger.Level.Warning, "Legacy communication port not set, starting legacy kDrive client without port argument.");
                    System.Diagnostics.Process.Start("kDrive_client.exe");
                }
                else
                {
                    Logger.Log(Logger.Level.Info, $"Starting legacy kDrive client with communication port: {legacyCommPort}");
                    System.Diagnostics.Process.Start("kDrive_client.exe", legacyCommPort.ToString());
                }
            }
            catch (Exception ex)
            {
                Logger.Log(Logger.Level.Error, $"Failed to start legacy kDrive client: {ex.Message}");
            }
        }
    }
}
