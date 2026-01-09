using CommunityToolkit.WinUI;
using H.NotifyIcon;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI;
using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Security;
using System.Threading.Tasks;
using Windows.Graphics;
using WinRT.Interop;

namespace Infomaniak.kDrive
{
    public static class Utility
    {
        public static async Task RunOnUIThread(Action action)
        {
            var dispatcher = AppModel.UIThreadDispatcher;
            if (dispatcher.HasThreadAccess)
            {
                action();
            }
            else
            {
                TaskCompletionSource taskCompletionSource = new TaskCompletionSource();
                await dispatcher.EnqueueAsync(() =>
                {
                    try
                    {
                        action();
                        taskCompletionSource.SetResult();
                    }
                    catch (Exception ex)
                    {
                        taskCompletionSource.SetException(ex);
                    }
                });
                await taskCompletionSource.Task;
            }
        }

        public static bool OpenFolderSecurely(string folderPath)
        {
            // Validate input
            if (string.IsNullOrWhiteSpace(folderPath))
            {
                Logger.Log(Logger.Level.Warning, "Cannot open the FolderPath which is null or empty.");
                return false;
            }

            // Prevent UNC paths
            if (folderPath.StartsWith(@"\\", StringComparison.OrdinalIgnoreCase))
            {
                Logger.Log(Logger.Level.Warning, $"Access to UNC paths is restricted ({folderPath}).");
                return false;
            }

            string fullPath;
            try
            {
                fullPath = Path.GetFullPath(folderPath);
            }
            catch (Exception ex)
            {
                Logger.Log(Logger.Level.Error, $"Invalid path format provided ({folderPath}): {ex.Message}");
                return false;
            }

            if (!Directory.Exists(fullPath))
            {
                Logger.Log(Logger.Level.Warning, $"The specified folder does not exist: {fullPath}");
                return false;
            }

            ProcessStartInfo startInfo = new ProcessStartInfo
            {
                FileName = "explorer.exe",
                Arguments = $"\"{fullPath}\"",
                UseShellExecute = true,
                CreateNoWindow = true
            };

            Process.Start(startInfo);
            return true;
        }

        public static class DpiHelper
        {
            [DllImport("User32.dll")]
            private static extern uint GetDpiForWindow(IntPtr hWnd);

            public static double GetScaleForWindow(IntPtr hWnd)
            {
                uint dpi = GetDpiForWindow(hWnd);
                return dpi / 96.0; // 96 DPI = 100%
            }
        }
        public static void SetWindowProperties(Window window, int width, int height, bool resizable)
        {
            var hWnd = WinRT.Interop.WindowNative.GetWindowHandle(window);
            var windowId = Win32Interop.GetWindowIdFromWindow(hWnd);
            var appWindow = AppWindow.GetFromWindowId(windowId);
            if (appWindow != null && appWindow.Presenter is OverlappedPresenter presenter)
            {
                presenter.IsMaximizable = true;
                presenter.IsMinimizable = true;
                presenter.IsResizable = resizable;

                // Use the RasterizationScale to scale the desired size
                double scale = DpiHelper.GetScaleForWindow(hWnd);

                int scaledWidth = (int)(width * scale);
                int scaledHeight = (int)(height * scale);
                presenter.PreferredMinimumWidth = scaledWidth;
                presenter.PreferredMinimumHeight = scaledHeight;
                appWindow.Resize(new SizeInt32(scaledWidth, scaledHeight));
            }
        }
        public static string GetLocalizedString(string key)
        {
            return GetLocalizedString(key, null);
        }

        public static string GetLocalizedString(string key, params object?[]? args)
        {
            var resourceLoader = Windows.ApplicationModel.Resources.ResourceLoader.GetForViewIndependentUse();
            string? localizedString = resourceLoader.GetString(key);

            if (localizedString is null || localizedString.Length == 0)
            {
                Logger.Log(Logger.Level.Error, $"Missing localization for key: {key} in current culture {System.Globalization.CultureInfo.CurrentUICulture.Name}");
            }

            // Replace literal \r\n with real newlines
            localizedString = localizedString.Replace("\\r\\n", Environment.NewLine);

            // Format the string if arguments are provided
            if (args != null && args.Length > 0)
            {
                localizedString = string.Format(localizedString, args);
            }

            return localizedString;
        }

        // Convert any enum value to a string
        public static string ToString(Enum value)
        {
            return value.ToString();
        }

        public static void SetEnumComboBoxSelection<TEnum>(ComboBox comboBox, TEnum value) where TEnum : struct, Enum
        {
            foreach (var item in comboBox.Items)
            {
                if (item is ComboBoxItem comboBoxItem && comboBoxItem.Tag is string tagString && Enum.TryParse<TEnum>(tagString, out TEnum itemValue) && itemValue.Equals(value))
                {
                    comboBox.SelectedItem = comboBoxItem;
                    return;
                }
            }
        }

        public static ContentDialog GetContentDialog(XamlRoot xamlRoot, string xuid, ContentDialogButton contentDialogButton = ContentDialogButton.Primary)
        {
            ContentDialog dialog = new ContentDialog();

            dialog.XamlRoot = xamlRoot;
            dialog.Title = Utility.GetLocalizedString(xuid + "/Title");
            dialog.PrimaryButtonText = Utility.GetLocalizedString(xuid + "/PrimaryButtonText");
            dialog.SecondaryButtonText = Utility.GetLocalizedString(xuid + "/SecondaryButtonText");
            dialog.DefaultButton = contentDialogButton;
            dialog.Content = Utility.GetLocalizedString(xuid + "/Content");
            return dialog;
        }

        public static async Task<ContentDialogResult> ShowContentDialogAsync(XamlRoot xamlRoot, string xuid, ContentDialogButton contentDialogButton = ContentDialogButton.Primary)
        {
            var result = await GetContentDialog(xamlRoot, xuid, contentDialogButton).ShowAsync();
            return result;
        }

        public static string? ToBase64String(string? data)
        {
            if (data is null)
                return null;
            return Convert.ToBase64String(System.Text.Encoding.UTF8.GetBytes(data));
        }

        public static bool IsSubPathOf(string path, string prefix)
        {
            if (!path.StartsWith(prefix))
                return false;

            if (path.Length == prefix.Length)
                return true;

            char nextChar = path[prefix.Length];
            return nextChar == Path.DirectorySeparatorChar || nextChar == Path.AltDirectorySeparatorChar;
        }

        public static Frame? GetFrame(Control control)
        {
            DependencyObject? parent = control;
            while (parent is not null)
            {
                if (parent is Frame frame)
                    return frame;

                parent = VisualTreeHelper.GetParent(parent);
            }
            return null;
        }

        [DllImport("user32.dll")]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool SetForegroundWindow(IntPtr hWnd);

        public static void BringCurrentWindowToFront()
        {
            Logger.Log(Logger.Level.Info, "Bringing current window to front");

            App? app = Application.Current as App;

            if (app?.CurrentWindow is null)
            {
                Logger.Log(Logger.Level.Warning, "Cannot bring window to front: Application?.Current?.CurrentWindow is null");
                return;
            }
            app.CurrentWindow.Show();
            app.CurrentWindow.Activate();
            var hWnd = WindowNative.GetWindowHandle(app.CurrentWindow);
            if (hWnd == IntPtr.Zero)
            {
                Logger.Log(Logger.Level.Warning, "Cannot bring window to front: hWnd is zero");
                return;
            }

            SetForegroundWindow(hWnd);
        }
    }
}





