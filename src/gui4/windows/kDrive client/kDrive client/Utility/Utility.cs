/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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
using CommunityToolkit.WinUI;
using H.NotifyIcon;
using Infomaniak.kDrive.Analytics;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI;
using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using System;
using System.Collections.Generic;
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
        public static async Task<T> RunOnUIThread<T>(Func<Task<T>> action)
        {
            var dispatcher = AppModel.UIThreadDispatcher;

            if (dispatcher.HasThreadAccess)
            {
                return await action().ConfigureAwait(false);
            }

            TaskCompletionSource<T> tcs = new(TaskCreationOptions.RunContinuationsAsynchronously);

            await dispatcher.EnqueueAsync(async () =>
            {
                try
                {
                    T result = await action().ConfigureAwait(false);
                    tcs.SetResult(result);
                }
                catch (Exception ex)
                {
                    tcs.SetException(ex);
                }
            }).ConfigureAwait(false);

            return await tcs.Task.ConfigureAwait(false);
        }

        public static async Task RunOnUIThread(Func<Task> action)
        {
            var dispatcher = AppModel.UIThreadDispatcher;

            if (dispatcher.HasThreadAccess)
            {
                await action();
                return;
            }

            await dispatcher.EnqueueAsync(async () =>
            {
                await action();
            });
        }

        public static Task RunOnUIThread(Action action)
        {
            return RunOnUIThread(() =>
            {
                action();
                return Task.CompletedTask;
            });
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
            private const int WM_DPICHANGED = 0x02E0;
            private const int GWLP_WNDPROC = -4;

            [DllImport("User32.dll")]
            private static extern uint GetDpiForWindow(IntPtr hWnd);

            [DllImport("User32.dll")]
            private static extern IntPtr CallWindowProc(IntPtr lpPrevWndFunc, IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);

            [DllImport("User32.dll", EntryPoint = "SetWindowLongPtrW")]
            private static extern IntPtr SetWindowLongPtr(IntPtr hWnd, int nIndex, IntPtr dwNewLong);

            [DllImport("User32.dll", EntryPoint = "GetWindowLongPtrW")]
            private static extern IntPtr GetWindowLongPtr(IntPtr hWnd, int nIndex);

            private delegate IntPtr WndProcDelegate(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);
            private sealed record class DpiSubclassData(IntPtr OriginalWndProc, WndProcDelegate WndProc, GCHandle WndProcHandle);
            private static readonly Dictionary<IntPtr, DpiSubclassData> _dpiSubclassDataByHwnd = [];
            private static readonly object _dpiSubclassLock = new();

            /// <summary>
            /// Raised when a WM_DPICHANGED message is received, after the window has been resized.
            /// </summary>
            public static event EventHandler<double>? DpiChanged;

            public static double GetScaleForWindow(IntPtr hWnd)
            {
                uint dpi = GetDpiForWindow(hWnd);
                return dpi / 96.0; // 96 DPI = 100%
            }

            /// <summary>
            /// Subclasses the window to listen for WM_DPICHANGED and automatically re-scale on DPI changes.
            /// </summary>
            public static bool RegisterDpiChangeHandler(IntPtr hWnd, AppWindow appWindow, int baseWidth, int baseHeight)
            {
                lock (_dpiSubclassLock)
                {
                    if (_dpiSubclassDataByHwnd.ContainsKey(hWnd))
                    {
                        return false;
                    }

                    IntPtr originalWndProc = GetWindowLongPtr(hWnd, GWLP_WNDPROC);

                    // Must be stored in a field to prevent garbage collection of the delegate.
                    WndProcDelegate newWndProc = (hwnd, msg, wParam, lParam) =>
                    {
                        if (msg == WM_DPICHANGED)
                        {
                            double newScale = (wParam.ToInt32() & 0xFFFF) / 96.0;
                            int scaledWidth = (int)(baseWidth * newScale);
                            int scaledHeight = (int)(baseHeight * newScale);

                            if (appWindow.Presenter is OverlappedPresenter p)
                            {
                                p.PreferredMinimumWidth = scaledWidth;
                                p.PreferredMinimumHeight = scaledHeight;
                            }

                            // The lParam contains a pointer to a RECT with the suggested new window position/size.
                            var suggestedRect = Marshal.PtrToStructure<RECT>(lParam);
                            appWindow.MoveAndResize(new RectInt32(
                                suggestedRect.Left,
                                suggestedRect.Top,
                                suggestedRect.Right - suggestedRect.Left,
                                suggestedRect.Bottom - suggestedRect.Top));

                            DpiChanged?.Invoke(null, newScale);

                            return IntPtr.Zero;
                        }

                        return CallWindowProc(originalWndProc, hwnd, msg, wParam, lParam);
                    };

                    // Pin the delegate to prevent GC collection
                    GCHandle wndProcHandle = GCHandle.Alloc(newWndProc);
                    _dpiSubclassDataByHwnd[hWnd] = new DpiSubclassData(originalWndProc, newWndProc, wndProcHandle);
                    SetWindowLongPtr(hWnd, GWLP_WNDPROC, Marshal.GetFunctionPointerForDelegate(newWndProc));
                }

                return true;
            }

            public static void UnregisterDpiChangeHandler(IntPtr hWnd)
            {
                DpiSubclassData? dpiSubclassData;
                lock (_dpiSubclassLock)
                {
                    if (!_dpiSubclassDataByHwnd.Remove(hWnd, out dpiSubclassData))
                    {
                        return;
                    }
                }

                SetWindowLongPtr(hWnd, GWLP_WNDPROC, dpiSubclassData.OriginalWndProc);
                if (dpiSubclassData.WndProcHandle.IsAllocated)
                {
                    dpiSubclassData.WndProcHandle.Free();
                }
            }

            [StructLayout(LayoutKind.Sequential)]
            private struct RECT
            {
                public int Left;
                public int Top;
                public int Right;
                public int Bottom;
            }
        }

        /// <summary>
        /// Configures window presenter properties and registers a WM_DPICHANGED listener for automatic DPI scaling.
        /// </summary>
        public enum WindowResizeOptions
        {
            None,
            AllowResize = 1,
            AllowMinimize = 2,
        }
        public static void SetWindowProperties(Window window, int width, int height, WindowResizeOptions resizeOptions)
        {
            var hWnd = WinRT.Interop.WindowNative.GetWindowHandle(window);
            var windowId = Win32Interop.GetWindowIdFromWindow(hWnd);
            var appWindow = AppWindow.GetFromWindowId(windowId);
            if (appWindow != null && appWindow.Presenter is OverlappedPresenter presenter)
            {
                presenter.IsMaximizable = resizeOptions.HasFlag(WindowResizeOptions.AllowResize);
                presenter.IsMinimizable = resizeOptions.HasFlag(WindowResizeOptions.AllowMinimize);
                presenter.IsResizable = resizeOptions.HasFlag(WindowResizeOptions.AllowResize);

                // Use the RasterizationScale to scale the desired size
                double scale = DpiHelper.GetScaleForWindow(hWnd);

                int scaledWidth = (int)(width * scale);
                int scaledHeight = (int)(height * scale);
                presenter.PreferredMinimumWidth = scaledWidth;
                presenter.PreferredMinimumHeight = scaledHeight;
                appWindow.Resize(new SizeInt32(scaledWidth, scaledHeight));
                appWindow.SetIcon("Assets\\kDrive.ico");

                // Subclass the window to automatically handle DPI changes
                if (DpiHelper.RegisterDpiChangeHandler(hWnd, appWindow, width, height))
                {
                    window.Closed += OnWindowClosed;
                }
            }
        }

        private static void OnWindowClosed(object sender, WindowEventArgs args)
        {
            var window = sender as Window;
            if (window is null)
                return;
            window.Closed -= OnWindowClosed;
            var hWnd = WinRT.Interop.WindowNative.GetWindowHandle(window);
            var windowId = Win32Interop.GetWindowIdFromWindow(hWnd);
            DpiHelper.UnregisterDpiChangeHandler(hWnd);
        }

        public static void CenterWindow(Window window)
        {
            IntPtr hWnd = WindowNative.GetWindowHandle(window);
            WindowId windowId = Win32Interop.GetWindowIdFromWindow(hWnd);

            if (AppWindow.GetFromWindowId(windowId) is AppWindow appWindow &&
                DisplayArea.GetFromWindowId(windowId, DisplayAreaFallback.Nearest) is DisplayArea displayArea)
            {
                PointInt32 CenteredPosition = appWindow.Position;
                CenteredPosition.X = displayArea.WorkArea.X + (displayArea.WorkArea.Width - appWindow.Size.Width) / 2;
                CenteredPosition.Y = displayArea.WorkArea.Y + (displayArea.WorkArea.Height - appWindow.Size.Height) / 2;
                appWindow.Move(CenteredPosition);
            }
        }

        public static void SetWindowCurrentSize(Window window, int width, int height)
        {
            var hWnd = WinRT.Interop.WindowNative.GetWindowHandle(window);
            var windowId = Win32Interop.GetWindowIdFromWindow(hWnd);
            var appWindow = AppWindow.GetFromWindowId(windowId);
            if (appWindow is not null)
            {
                // Use the RasterizationScale to scale the desired size
                double scale = DpiHelper.GetScaleForWindow(hWnd);
                int scaledWidth = (int)(width * scale);
                int scaledHeight = (int)(height * scale);
                appWindow.Resize(new SizeInt32(scaledWidth, scaledHeight));
            }
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
            BringWindowToFront(app.CurrentWindow);
        }

        public static void BringWindowToFront(Window window)
        {
            Logger.Log(Logger.Level.Info, "Bringing current window to front");
            if (!window.Visible)
            {
                window.Activate();
            }
            else
            {
                var hWnd = WindowNative.GetWindowHandle(window);
                if (hWnd == IntPtr.Zero)
                {
                    Logger.Log(Logger.Level.Warning, "Cannot bring window to front: hWnd is zero");
                    return;
                }

                SetForegroundWindow(hWnd);
            }
            window.Show();
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


        public static void ShowUnexpectedErrorTeachingTip()
        {
            Logger.Log(Logger.Level.Error, "Showing unexpected error TeachingTip");
            App.ServiceProvider.GetRequiredService<IAnalyticsService>().TrackOther(Analytics.Keys.Category.UnexpectedErrorTeachingTip, Analytics.Keys.EventName.Displayed);
            ShowTeachingTip(Localizer.Instance.GetString("unexpectedErrorTeachingTipTitle"), Localizer.Instance.GetString("unexpectedErrorTeachingTipContent"));
        }

        private static TeachingTip? _currentTeachingTip;
        private static DispatcherQueueTimer? _autoCloseTimer;

        public static void ShowTeachingTip(string title, string? content = null, TimeSpan? maxDuration = null /* default is 5s*/)
        {
            if (App.Current is not App app || app.CurrentWindow is null)
            {
                Logger.Log(Logger.Level.Error, "Cannot show TeachingTip: App.Current or CurrentWindow is null");
                return;
            }

            // Close previous tip if any
            CloseCurrentTeachingTip();

            XamlRoot xamlRoot = app.CurrentWindow.Content.XamlRoot;

            var teachingTip = new TeachingTip
            {
                XamlRoot = xamlRoot,
                Title = title,
                Subtitle = content,
                PreferredPlacement = TeachingTipPlacementMode.Bottom,
                IsLightDismissEnabled = true,
            };

            teachingTip.Closed += TeachingTip_Closed;

            if (xamlRoot.Content is Panel panel)
            {
                panel.Children.Add(teachingTip);
            }

            _currentTeachingTip = teachingTip;
            teachingTip.IsOpen = true;

            // ---- Auto close after 5 seconds ----
            _autoCloseTimer = DispatcherQueue.GetForCurrentThread().CreateTimer();
            _autoCloseTimer.Interval = maxDuration ?? TimeSpan.FromSeconds(5);
            _autoCloseTimer.IsRepeating = false;
            _autoCloseTimer.Tick += TeachingTipAutoCloseTimer_Tick;
            _autoCloseTimer.Start();
        }

        private static void TeachingTipAutoCloseTimer_Tick(DispatcherQueueTimer sender, object args)
        {
            CloseCurrentTeachingTip();
        }

        private static void CloseCurrentTeachingTip()
        {
            if (_autoCloseTimer is not null)
            {
                _autoCloseTimer.Stop();
                _autoCloseTimer.Tick -= TeachingTipAutoCloseTimer_Tick;
                _autoCloseTimer = null;
            }

            if (_currentTeachingTip is null)
                return;

            _currentTeachingTip.IsOpen = false;
        }

        private static void TeachingTip_Closed(TeachingTip sender, TeachingTipClosedEventArgs args)
        {
            if (VisualTreeHelper.GetParent(sender) is Panel panel)
            {
                panel.Children.Remove(sender);
            }

            sender.Closed -= TeachingTip_Closed;

            if (ReferenceEquals(_currentTeachingTip, sender))
            {
                _currentTeachingTip = null;
            }
        }
    }

    public static class EnumExtensions
    {
        public static string ToCamelCase(this Enum value)
        {
            string name = value.ToString();

            if (string.IsNullOrEmpty(name))
                return name;

            return char.ToLowerInvariant(name[0]) + name[1..];
        }
    }
}


