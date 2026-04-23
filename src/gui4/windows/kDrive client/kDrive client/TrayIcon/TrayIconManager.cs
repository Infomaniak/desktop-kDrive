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

using DynamicData;
using DynamicData.Binding;
using H.NotifyIcon;
using Infomaniak.kDrive.CustomControls.Errors;
using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Input;
using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Threading;
using Windows.UI.ViewManagement;
using static Infomaniak.kDrive.App;

namespace Infomaniak.kDrive.TrayIcon
{
    public partial class TrayIconManager : IDisposable
    {
        private TaskbarIcon? _trayIcon;
        private string _currentIcon = "taskbar-ico";
        private UISettings? _uiSettings;
        private readonly AppModel _appModel;
        private readonly List<IDisposable> _subscriptions = new List<IDisposable>();

        public void Initialize()
        {
            Logger.Log(Logger.Level.Info, "Initializing TrayIconManager");

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
                SetIconNeutral();
            }
            else
            {
                Logger.Log(Logger.Level.Error, "TrayIcon resource not found in application resources, unable to initialize tray icon.");
                (Application.Current as App)?.CurrentWindow?.Show();
            }

            _uiSettings = new UISettings();
            _uiSettings.ColorValuesChanged += UISettings_ColorValuesChanged;
        }

        public TrayIconManager(AppModel appModel)
        {
            _appModel = appModel;

            // Subscribe to changes in the syncs collection, their statuses, and their errors
            _subscriptions.Add(_appModel.AllSyncs
                .ToObservableChangeSet()
                .AutoRefresh(sync => sync.SyncStatus) // react when a sync's Status changes
                .AutoRefreshOnObservable(sync => sync.SyncErrors.ToObservableChangeSet()) // react when any sync's errors change
                .Subscribe(_ => UpdateTrayIcon()));

            // Subscribe to changes in the available update
            _subscriptions.Add(_appModel.Settings.UpdateManager.WhenPropertyChanged(updateManager => updateManager.AvailableUpdate)
                .Subscribe(_ => UpdateTrayIcon()));
        }

        private void UpdateTrayIcon()
        {

            if (_appModel.AllSyncs.Any(sync => sync.SyncStatus == SyncStatus.Running || sync.SyncStatus == SyncStatus.Paused))
            {
                SetIconSync();
                return;
            }

            if (_appModel.AllSyncs.Any(sync => sync.SyncErrors.Any(err => ErrorFactory.GetErrorCardInfos(err)?.Meta.ShowInSystemTray == true)))
            {
                SetIconError();
                return;
            }

            if (_appModel.Settings.UpdateManager.AvailableUpdate is not null)
            {
                SetIconNotification();
                return;
            }

            if (_appModel.AllSyncs.All(sync => sync.SyncStatus == SyncStatus.Stopped || sync.SyncStatus == SyncStatus.Offline))
            {
                SetIconPause();
                return;
            }

            SetIconOk();
        }

        private async void UISettings_ColorValuesChanged(UISettings sender, object args)
        {
            Logger.Log(Logger.Level.Extended, "System theme or color changed, updating tray icon.");
            await Utility.RunOnUIThread(() => RefreshTheme());
        }

        public void SetIconOk()
        {
            Logger.Log(Logger.Level.Debug, "Setting tray icon to 'ok' state.");
            SetIcon("taskbar-ico");
        }
        public void SetIconSync()
        {
            Logger.Log(Logger.Level.Debug, "Setting tray icon to 'sync' state.");
            SetIcon("taskbar-ico-sync");
        }
        public void SetIconError()
        {
            Logger.Log(Logger.Level.Debug, "Setting tray icon to 'error' state.");
            SetIcon("taskbar-ico-error");
        }
        public void SetIconPause()
        {
            Logger.Log(Logger.Level.Debug, "Setting tray icon to 'pause' state.");
            SetIcon("taskbar-ico-pause");
        }
        public void SetIconNotification()
        {
            Logger.Log(Logger.Level.Debug, "Setting tray icon to 'notification' state.");
            SetIcon("taskbar-ico-notification");
        }
        public void SetIconNeutral()
        {
            Logger.Log(Logger.Level.Debug, "Setting tray icon to 'neutral' state.");
            SetIcon("taskbar-ico");
        }

        private async void ShowWindowCommand_ExecuteRequested(object? sender, ExecuteRequestedEventArgs args)
        {
            Logger.Log(Logger.Level.Info, "ShowWindowCommand executed - showing and activating main window");
            if (Application.Current is App app)
            {
                app.CreateWindow(CreateWindowOptions.Foreground);
            }
            await App.ServiceProvider.GetRequiredService<IServerCommService>().ActivateLoadInfo(CancellationToken.None);
        }

        private void ExitApplicationCommand_ExecuteRequested(object? sender, ExecuteRequestedEventArgs args)
        {
            Logger.Log(Logger.Level.Info, "ExitApplicationCommand executed - exiting application");
            _trayIcon?.Dispose();
            App.ExitApplicationAndShutdownServer();
        }

        private void SetIcon(string fileName)
        {
            if (_trayIcon is null)
                return;

            try
            {
                _currentIcon = fileName;
                var imagePath = Path.Combine(AppContext.BaseDirectory, "Assets", "logo", $"{_currentIcon}{GetThemeSuffix()}.ico");
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

        private void RefreshTheme()
        {
            Logger.Log(Logger.Level.Info, "Refreshing tray icon theme due to theme change");
            SetIcon(_currentIcon);
        }

        private static string GetThemeSuffix()
        {
            bool isDark = App.Current.RequestedTheme == ApplicationTheme.Dark;
            return isDark ? "-dark" : "-light";
        }

        public void Dispose()
        {
            foreach (var subscription in _subscriptions)
            {
                subscription.Dispose();
            }
        }
    }
}