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
using Microsoft.UI;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Input;
using System;
using System.Drawing;
using System.IO;
using Windows.System;
using Microsoft.UI.Windowing;

namespace Infomaniak.kDrive.TrayIcon
{
    public class TrayIconManager
    {
        private TaskbarIcon? _trayIcon;
        private Window? _window;
        private bool _handleClosedEvents = true;

        public void Initialize(Window window)
        {
            Logger.Log(Logger.Level.Info, "Initializing TrayIconManager");
            _window = window;

            // Find resources from the app's main window or application resources
            if (Application.Current.Resources["ShowWindowCommand"] is XamlUICommand showCommand)
            {
                showCommand.ExecuteRequested -= ShowWindowCommand_ExecuteRequested; // Avoid double subscription
                showCommand.ExecuteRequested += ShowWindowCommand_ExecuteRequested;
            }
            else
            {
                Logger.Log(Logger.Level.Error, "ShowWindowCommand not found in application resources.");
            }

            if (Application.Current.Resources["ExitApplicationCommand"] is XamlUICommand exitCommand)
            {
                exitCommand.ExecuteRequested -= ExitApplicationCommand_ExecuteRequested;
                exitCommand.ExecuteRequested += ExitApplicationCommand_ExecuteRequested;
            }
            else
            {
                Logger.Log(Logger.Level.Error, "ExitApplicationCommand not found in application resources.");
            }

            if (Application.Current.Resources["TrayIcon"] is TaskbarIcon trayIcon)
            {
                _trayIcon = trayIcon;
                _trayIcon.ForceCreate();

                // Set initial icon
                SetIcon_ok();
            }
            else
            {
                Logger.Log(Logger.Level.Error, "TrayIcon resource not found in application resources, unable to initialize tray icon.");
                _window.Show();
            }

            _window.Closed += (sender, args) =>
            {
                if (_handleClosedEvents)
                {
                    args.Handled = true;
                    _window.Hide();
                }
            };
        }
        public void SetIcon_ok()
        {
            Logger.Log(Logger.Level.Debug, "Setting tray icon to 'ok' state.");
            SetIcon("state-ok.ico");
        }
        public void SetIcon_sync()
        {
            Logger.Log(Logger.Level.Debug, "Setting tray icon to 'sync' state.");
            SetIcon("state-sync.ico");
        }
        public void SetIcon_error()
        {
            Logger.Log(Logger.Level.Debug, "Setting tray icon to 'error' state.");
            SetIcon("state-error.ico");
        }
        public void SetIcon_neutral()
        {
            Logger.Log(Logger.Level.Debug, "Setting tray icon to 'neutral' state.");
            SetIcon("state-neutral.ico");
        }

        private void ShowWindowCommand_ExecuteRequested(object? sender, ExecuteRequestedEventArgs args)
        {
            // Bring to front
            Logger.Log(Logger.Level.Info, "ShowWindowCommand executed - showing and activating main window");
            var hWnd = WinRT.Interop.WindowNative.GetWindowHandle(_window);
            var windowId = Win32Interop.GetWindowIdFromWindow(hWnd);
            var appWindow = AppWindow.GetFromWindowId(windowId);
            if (appWindow.Presenter is OverlappedPresenter presenter)
            {
                presenter.IsMaximizable = false;
                presenter.IsMinimizable = true;
                presenter.IsResizable = true;
                appWindow.Resize(new Windows.Graphics.SizeInt32(1350, 795));
                presenter.Minimize();
                presenter.Restore();
            }

            _window?.Show();
            _window?.Activate();
            SetIcon_ok();

        }
        private void ExitApplicationCommand_ExecuteRequested(object? sender, ExecuteRequestedEventArgs args)
        {
            Logger.Log(Logger.Level.Info, "ExitApplicationCommand executed - exiting application"); 
            _handleClosedEvents = false;
            _trayIcon?.Dispose();

            _window?.Close();

            // If window was never created, exit directly
            if (_window == null)
            {
                Environment.Exit(0);
            }
        }

        private void SetIcon(string fileName)
        {
            if (_trayIcon == null) return;

            try
            {
                var imagePath = Path.Combine(AppContext.BaseDirectory, "Assets", "StatusIcons", fileName);
                using var bitmap = new Bitmap(imagePath);
                var iconHandle = bitmap.GetHicon();
                var icon = Icon.FromHandle(iconHandle);
                _trayIcon.UpdateIcon(icon);
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine($"Failed to set tray icon: {ex.Message}");
            }
        }
    }
}