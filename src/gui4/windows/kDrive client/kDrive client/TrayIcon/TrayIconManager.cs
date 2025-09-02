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

namespace KDrive.TrayIcon
{
    public class TrayIconManager
    {
        private TaskbarIcon? _trayIcon;
        private Window? _window;
        private bool _handleClosedEvents = true;

        public void Initialize(Window window)
        {
            _window = window;

            // Find resources from the app's main window or application resources
            if (Application.Current.Resources["ShowWindowCommand"] is XamlUICommand showCommand)
            {
                showCommand.ExecuteRequested -= ShowWindowCommand_ExecuteRequested; // Avoid double subscription
                showCommand.ExecuteRequested += ShowWindowCommand_ExecuteRequested;
            }

            if (Application.Current.Resources["ExitApplicationCommand"] is XamlUICommand exitCommand)
            {
                exitCommand.ExecuteRequested -= ExitApplicationCommand_ExecuteRequested;
                exitCommand.ExecuteRequested += ExitApplicationCommand_ExecuteRequested;
            }

            if (Application.Current.Resources["TrayIcon"] is TaskbarIcon trayIcon)
            {
                _trayIcon = trayIcon;
                _trayIcon.ForceCreate();

                // Set initial icon
                SetIcon_ok();
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
            SetIcon("state-ok.ico");
        }
        public void SetIcon_sync()
        {
            SetIcon("state-sync.ico");
        }
        public void SetIcon_error()
        {
            SetIcon("state-error.ico");
        }
        public void SetIcon_neutral()
        {
            SetIcon("state-neutral.ico");
        }
        private void ShowWindowCommand_ExecuteRequested(object? sender, ExecuteRequestedEventArgs args)
        {
            // Bring to front
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
            _window.SizeChanged += (s, e) =>
            {
                if (_window != null && appWindow != null)
                {
                    if (e.Size.Width < 1350 && e.Size.Height < 795)
                    {
                        appWindow.Resize(new Windows.Graphics.SizeInt32(1350, 795));
                    }
                    else if (e.Size.Width < 1350)
                    {
                        appWindow.Resize(new Windows.Graphics.SizeInt32(1350, (int)e.Size.Height));
                    }
                    else if (e.Size.Height < 795)
                    {
                        appWindow.Resize(new Windows.Graphics.SizeInt32((int)e.Size.Width, 795));
                    }
                }
            };

            _window?.Show();
            _window?.Activate();
            SetIcon_ok();

        }
        private void ExitApplicationCommand_ExecuteRequested(object? sender, ExecuteRequestedEventArgs args)
        {
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