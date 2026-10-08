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
using System.Runtime.InteropServices;
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
        private readonly Timer _toolTipCycleTimer;
        private int _toolTipCycleIndex;

        private const int ToolTipMaxLength = 127;
        private static readonly TimeSpan ToolTipCycleInterval = TimeSpan.FromSeconds(3);

        public void Initialize()
        {
            Logger.LogInfo("Initializing TrayIconManager");

            // Find resources from the app's main window or application resources
            if (Application.Current.Resources["ShowWindowCommand"] is XamlUICommand showCommand)
            {
                showCommand.ExecuteRequested -= ShowWindowCommand_ExecuteRequested; // Avoid double subscription
                showCommand.ExecuteRequested += ShowWindowCommand_ExecuteRequested;
            }
            else
            {
                Logger.LogError("ShowWindowCommand not found in application resources.");
            }


            if (Application.Current.Resources["OpenSettingsCommand"] is XamlUICommand settingsCommand)
            {
                settingsCommand.ExecuteRequested -= OpenSettingsCommand_ExecuteRequested;
                settingsCommand.ExecuteRequested += OpenSettingsCommand_ExecuteRequested;
            }
            else
            {
                Logger.LogError("OpenSettingsCommand not found in application resources.");
            }

            if (Application.Current.Resources["ExitApplicationCommand"] is XamlUICommand exitCommand)
            {
                exitCommand.ExecuteRequested -= ExitApplicationCommand_ExecuteRequested;
                exitCommand.ExecuteRequested += ExitApplicationCommand_ExecuteRequested;
            }
            else
            {
                Logger.LogError("ExitApplicationCommand not found in application resources.");
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
                Logger.LogError("TrayIcon resource not found in application resources, unable to initialize tray icon.");
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
            _subscriptions.Add(_appModel.Settings.UpdateManager.WhenPropertyChanged(updateManager => updateManager.ShowNotification)
                .Subscribe(_ => UpdateTrayIcon()));

            // subscribe to changes on the mass deletion controller
            _subscriptions.Add(_appModel.ManyDeletesController.WhenPropertyChanged(manyDeletesController => manyDeletesController.CurrentManyDeleteNotification)
                .Subscribe(_ => UpdateTrayIcon()));

            _appModel.SelectedSyncChanged += AppModel_SelectedSyncChanged;

            _toolTipCycleTimer = new Timer(ToolTipCycleTimer_Tick, null, ToolTipCycleInterval, ToolTipCycleInterval);
        }

        private void AppModel_SelectedSyncChanged(object? sender, AppModel.SelectedSyncChangedEventArgs e)
        {
            _toolTipCycleIndex = 0;
            _ = Utility.RunOnUIThread(() => UpdateToolTip());
        }

        private void ToolTipCycleTimer_Tick(object? state)
        {
            Interlocked.Increment(ref _toolTipCycleIndex);
            _ = Utility.RunOnUIThread(() => UpdateToolTip());
        }

        private void UpdateTrayIcon()
        {
            UpdateToolTip();

            if (!_appModel.IsInitialized)
            {
                SetIconNeutral();
                return;
            }


            if (_appModel.ManyDeletesController.CurrentManyDeleteNotification is not null)
            {
                SetIconError();
                return;
            }

            if (_appModel.AllSyncs.Any(sync => sync.SyncStatus == SyncStatus.Running))
            {
                SetIconSync();
                return;
            }

            if (_appModel.AllSyncs.Any(sync => sync.SyncErrors.Any(err => ErrorFactory.GetErrorCardInfos(err)?.Meta.ShowInSystemTray == true)))
            {
                SetIconError();
                return;
            }

            if (_appModel.AllSyncs.All(sync => sync.SyncStatus == SyncStatus.Paused || sync.SyncStatus == SyncStatus.Stopped || sync.SyncStatus == SyncStatus.Offline))
            {
                SetIconPause();
                return;
            }

            if (_appModel.Settings.UpdateManager.ShowNotification)
            {
                SetIconNotification();
                return;
            }

            SetIconNeutral();
        }


        private void UpdateToolTip()
        {
            if (_trayIcon is null)
                return;

            if (!_appModel.IsInitialized) return;

            var syncs = _appModel.AllSyncs.ToList();
            if (syncs.Count == 0)
            {
                _trayIcon.ToolTipText = "kDrive";
                return;
            }

            var selectedSync = _appModel.SelectedSync is { } selected && syncs.Contains(selected) ? selected : syncs[0];
            var otherSyncs = syncs.Where(sync => !ReferenceEquals(sync, selectedSync)).ToList();

            GetToolTipTextForSync(selectedSync, 1, 1, out string selectedLine);
            selectedLine = $"{selectedLine}";

            if (otherSyncs.Count == 0)
            {
                _trayIcon.ToolTipText = Truncate(selectedLine, ToolTipMaxLength);
                return;
            }

            var index = (int)((uint)Volatile.Read(ref _toolTipCycleIndex) % (uint)otherSyncs.Count);
            GetToolTipTextForSync(otherSyncs[index], index, otherSyncs.Count, out string otherLine);

            // Keep both lines within the system tooltip limit, giving priority to the selected sync
            selectedLine = Truncate(selectedLine, ToolTipMaxLength / 2);
            otherLine = Truncate(otherLine, ToolTipMaxLength - selectedLine.Length - 2);

            _trayIcon.ToolTipText = $"{selectedLine}\n\n{otherLine}";
        }

        private static string Truncate(string text, int maxLength)
        {
            if (text.Length <= maxLength)
                return text;
            return string.Concat(text.AsSpan(0, Math.Max(0, maxLength - 1)), "\u2026");
        }

        private void GetToolTipTextForSync(Sync sync, int index, int total, out string tooltipText)
        {

            tooltipText = $"{Path.GetFileName(sync.LocalPath)}";
            if (total > 1)
            {
                tooltipText = $"{tooltipText} ({index + 1}/{total})";
            }

            switch (sync.SyncStatus)
            {
                case SyncStatus.Running:
                    tooltipText += $"\n{Localizer.Instance.GetString("activitiesTitleInProgress")}";
                    break;
                case SyncStatus.Stopped:
                case SyncStatus.Paused:
                case SyncStatus.Error:
                    tooltipText += $"\n{Localizer.Instance.GetString("activitiesTitlePause")}";
                    break;
                case SyncStatus.Offline:
                    tooltipText += $"\n{Localizer.Instance.GetString("activitiesTitleOffline")}";
                    break;
                case SyncStatus.Idle:
                    tooltipText += $"\n{Localizer.Instance.GetString("activitiesTitleIdle")}";
                    break;
                default:
                    break;
            }
        }

        private async void UISettings_ColorValuesChanged(UISettings sender, object args)
        {
            Logger.LogExtended("System theme or color changed, updating tray icon.");
            await Utility.RunOnUIThread(() => RefreshTheme());
        }

        public void SetIconSync()
        {
            Logger.LogDebug("Setting tray icon to 'sync' state.");
            SetIcon("sync");
        }
        public void SetIconError()
        {
            Logger.LogDebug("Setting tray icon to 'error' state.");
            SetIcon("error");
        }
        public void SetIconPause()
        {
            Logger.LogDebug("Setting tray icon to 'pause' state.");
            SetIcon("pause");
        }
        public void SetIconNotification()
        {
            Logger.LogDebug("Setting tray icon to 'notification' state.");
            SetIcon("notif");
        }
        public void SetIconNeutral()
        {
            Logger.LogDebug("Setting tray icon to 'neutral' state.");
            SetIcon("neutral");
        }

        private async void ShowWindowCommand_ExecuteRequested(object? sender, ExecuteRequestedEventArgs args)
        {
            Logger.LogInfo("ShowWindowCommand executed - showing and activating main window");
            if (Application.Current is App app)
            {
                app.CreateWindow(CreateWindowOptions.Foreground);
            }
            await App.ServiceProvider.GetRequiredService<IServerCommService>().ActivateLoadInfo(CancellationToken.None);
        }

        private void ExitApplicationCommand_ExecuteRequested(object? sender, ExecuteRequestedEventArgs args)
        {
            Logger.LogInfo("ExitApplicationCommand executed - exiting application");
            _trayIcon?.Dispose();
            App.ExitApplicationAndShutdownServer();
        }

        private void OpenSettingsCommand_ExecuteRequested(XamlUICommand sender, ExecuteRequestedEventArgs args)
        {
            Logger.LogInfo("OpenSettingsCommand executed");
            if (Application.Current is App app)
            {
                app.CreateWindow(CreateWindowOptions.Foreground | CreateWindowOptions.CancelOnboarding | CreateWindowOptions.OpenSettings);
            }
        }

        [System.Runtime.InteropServices.DllImport("user32.dll", CharSet = CharSet.Auto)]
        extern static bool DestroyIcon(IntPtr handle);

        private void SetIcon(string fileName)
        {
            if (_trayIcon is null)
                return;

            try
            {
                _currentIcon = fileName;
                var imagePath = Path.Combine(AppContext.BaseDirectory, "Assets", "Custom", "Icons", "TrayIcons", $"{_currentIcon}{GetThemeSuffix()}.ico");
                using var bitmap = new Bitmap(imagePath);
                var iconHandle = bitmap.GetHicon();
                var icon = Icon.FromHandle(iconHandle);
                _trayIcon.UpdateIcon(icon);
                DestroyIcon(iconHandle);
            }
            catch (Exception ex)
            {
                Logger.LogWarning($"Failed to set tray icon: {ex.Message}",
                    "TrayIconManager: Failed to set tray icon");
            }
        }

        private void RefreshTheme()
        {
            Logger.LogInfo("Refreshing tray icon theme due to theme change");
            SetIcon(_currentIcon);
        }

        private static string GetThemeSuffix()
        {
            bool isDark = App.Current.RequestedTheme == ApplicationTheme.Dark;
            return isDark ? "-dark" : "-light";
        }

        public void Dispose()
        {
            _toolTipCycleTimer.Dispose();
            _appModel.SelectedSyncChanged -= AppModel_SelectedSyncChanged;
            foreach (var subscription in _subscriptions)
            {
                subscription.Dispose();
            }
        }
    }
}