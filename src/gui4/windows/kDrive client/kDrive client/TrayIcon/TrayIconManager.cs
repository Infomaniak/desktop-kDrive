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

namespace kDrive_client.TrayIcon
{
    public class TrayIconManager
    {
        private TaskbarIcon? _trayIcon;
        private Window? _window;
        private bool _handleClosedEvents = true;

        public void Initialize(Window? window)
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

            if (_window == null)
            {
                _window = new MainWindow();
                _window.Closed += (sender, args) =>
                {
                    if (_handleClosedEvents)
                    {
                        args.Handled = true;
                        _window.Hide();
                    }
                };
            }

            // Bring to front
            var hWnd = WinRT.Interop.WindowNative.GetWindowHandle(_window);
            var windowId = Win32Interop.GetWindowIdFromWindow(hWnd);
            var appWindow = AppWindow.GetFromWindowId(windowId);
            appWindow.Resize(new Windows.Graphics.SizeInt32(1200, 668));
            if (appWindow.Presenter is OverlappedPresenter presenter)
            {
                presenter.IsMaximizable = false;
                presenter.IsMinimizable = true;
                presenter.IsResizable = false;
                presenter.Minimize();
                presenter.Restore();
            }
            _window.Show();
            _window.Activate();
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
                var imagePath = Path.Combine(AppContext.BaseDirectory, "Assets", "status-icons", fileName);
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

        public void Dispose()
        {
            _trayIcon?.Dispose();
            _trayIcon = null;
        }
    }
}