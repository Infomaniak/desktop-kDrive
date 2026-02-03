using CommunityToolkit.WinUI;
using H.NotifyIcon;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI;
using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using Windows.Graphics;
using Windows.Storage;
using Windows.System;
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
        public static async Task OpenFileAsync(string filePath)
        {
            try
            {
                filePath = filePath.Replace('/', Path.DirectorySeparatorChar);
                StorageFile file = await StorageFile.GetFileFromPathAsync(filePath);
                await Launcher.LaunchFileAsync(file);
            }
            catch (Exception ex)
            {
                Logger.Log(Logger.Level.Error, $"Failed to open file {filePath}: {ex.Message}");
            }
        }

        public static async Task<bool> OpenFolderSecurely(string folderPath)
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

            if (!Directory.Exists(Path.GetDirectoryName(fullPath)))
            {
                Logger.Log(Logger.Level.Warning, $"The parent directory does not exist for the specified folder: {fullPath}");
                return false;
            }

            if (!Directory.Exists(fullPath))
            {
                await Launcher.LaunchFolderPathAsync(Path.GetDirectoryName(fullPath));
                return false;
            }

            await Launcher.LaunchFolderPathAsync(fullPath);
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
                try
                {
                    localizedString = string.Format(localizedString, args);
                }
                catch (Exception e)
                {
                    Logger.Log(Logger.Level.Error, $"Failed to format localized string: {localizedString} with args: {string.Join(", ", args)}. Error: {e.Message}");
                }
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

        public static ContentDialog GetContentDialog(XamlRoot xamlRoot, string xuid, ContentDialogButton defaultButton = ContentDialogButton.Primary)
        {
            ContentDialog dialog = new ContentDialog();

            dialog.XamlRoot = xamlRoot;
            dialog.Title = Utility.GetLocalizedString(xuid + "/Title");
            dialog.PrimaryButtonText = Utility.GetLocalizedString(xuid + "/PrimaryButtonText");
            dialog.SecondaryButtonText = Utility.GetLocalizedString(xuid + "/SecondaryButtonText");
            dialog.DefaultButton = defaultButton;
            dialog.Content = Utility.GetLocalizedString(xuid + "/Content");
            return dialog;
        }

        public static async Task<ContentDialogResult> ShowContentDialogAsync(XamlRoot xamlRoot, string xuid, ContentDialogButton defaultButton = ContentDialogButton.Primary)
        {
            var result = await GetContentDialog(xamlRoot, xuid, defaultButton).ShowAsync();
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
        public static string ObfuscateEmail(string? email)
        {
            // if > 5 characters before @, keep first 2 and last 2, else keep first and last
            if (email is null)
                return "";

            var atIndex = email.IndexOf('@');
            if (atIndex <= 0)
                return email; // invalid email

            var localPart = email.Substring(0, atIndex);
            var domainPart = email.Substring(atIndex);

            if (localPart.Length > 5)
                return localPart.Substring(0, 2) + new string('*', localPart.Length - 4) + localPart.Substring(localPart.Length - 2) + domainPart;
            else if (localPart.Length > 2)
                return localPart.Substring(0, 1) + new string('*', localPart.Length - 2) + localPart.Substring(localPart.Length - 1) + domainPart;
            else
                return new string('*', localPart.Length) + domainPart;
        }

        public static void ShowTeachingTip(XamlRoot xamlRoot, string xuid, Control? target = null)
        {
            var teachingTip = new TeachingTip
            {
                XamlRoot = xamlRoot,
                Title = GetLocalizedString($"{xuid}/Title"),
                Subtitle = GetLocalizedString($"{xuid}/Subtitle"),
                Content = new TextBlock
                {
                    Text = GetLocalizedString($"{xuid}/Content"),
                    TextWrapping = TextWrapping.Wrap
                },
                PreferredPlacement = TeachingTipPlacementMode.Bottom,
                IsLightDismissEnabled = true,
            };

            if (target != null)
            {
                teachingTip.Target = target;
            }

            // Attach to visual tree
            var rootPanel = (xamlRoot.Content as FrameworkElement);
            if (rootPanel is Panel panel)
            {
                panel.Children.Add(teachingTip);
            }

            teachingTip.IsOpen = true;
            teachingTip.Closed += TeachingTip_Closed;
        }

        private static void TeachingTip_Closed(TeachingTip sender, TeachingTipClosedEventArgs args)
        {
            // Detach from visual tree
            var parent = VisualTreeHelper.GetParent(sender);
            if (parent is Panel panel)
            {
                panel.Children.Remove(sender);
            }

            // Unsubscribe from event
            sender.Closed -= TeachingTip_Closed;
        }
    }
}





