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

using H.NotifyIcon;
using Infomaniak.kDrive.CustomControls;
using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Input;

namespace Infomaniak.kDrive
{
    public sealed partial class MainWindow : Window
    {
        private const int _defaultWidth = 1025;
        private const int _defaultHeight = 683;
        private const int _minimumWidth = 900;
        private const int _minimumHeight = 600;
        public AppNavigationView AppNavView { get { return NavView; } }
        public AppModel ViewModel { get; } = App.ServiceProvider.GetRequiredService<AppModel>();

        private int _manyDeletesNbFiles;
        public int ManyDeletesNbFiles
        {
            get => _manyDeletesNbFiles;
            set
            {
                if (_manyDeletesNbFiles == value)
                    return;
                _manyDeletesNbFiles = value;
                Bindings.Update();
            }
        }

        public MainWindow(Type? landingPageType = null)
        {
            InitializeComponent();
            AppNavView.LandingPageType = landingPageType;
            this.ExtendsContentIntoTitleBar = true;  // enable custom titlebar
            this.SetTitleBar(AppTitleBar);
            Utility.SetWindowProperties(this, _minimumWidth, _minimumHeight, Utility.WindowResizeOptions.AllowMinimize | Utility.WindowResizeOptions.AllowResize); // Set the minimum size and allow resizing
            Utility.SetWindowCurrentSize(this, _defaultWidth, _defaultHeight); // Default size sized for the navigation pane expanded state
            Utility.CenterWindow(this);
            AppModel.UIThreadDispatcher = Microsoft.UI.Dispatching.DispatcherQueue.GetForCurrentThread(); // Save the UI thread dispatcher for later use in view models
            AppWindow.TitleBar.PreferredTheme = Microsoft.UI.Windowing.TitleBarTheme.UseDefaultAppMode;
            ViewModel.PropertyChanged += ViewModel_PropertyChanged;
            Closed += MainWindow_Closed;
            Activated += MainWindow_Activated;
            this.Content.PointerPressed += OnPointerPressed;
            TrackMainContentReadiness();
        }

        // True once Content.Loaded has fired, meaning XamlRoot is safe to use (e.g. for ContentDialog).
        private bool _mainContentReady = false;

        private void TrackMainContentReadiness()
        {
            if (this.Content is not FrameworkElement contentRoot)
                return;

            if (contentRoot.IsLoaded)
            {
                _mainContentReady = true;
                return;
            }

            contentRoot.Loaded += MainContent_Loaded;
        }

        private async void MainContent_Loaded(object sender, RoutedEventArgs e)
        {
            if (this.Content is FrameworkElement contentRoot)
                contentRoot.Loaded -= MainContent_Loaded;

            _mainContentReady = true;
            UpdateControlsVisibility();
            await ProcessManyDeleteQueue();
        }

        private void OnPointerPressed(object sender, PointerRoutedEventArgs e)
        {
            var props = e.GetCurrentPoint(null).Properties;

            if (props.IsXButton1Pressed) // Mouse Back button
            {
                if (AppNavView?.Frame?.CanGoBack == true)
                {
                    AppNavView?.Frame?.GoBack();
                    e.Handled = true;
                }
            }
            else if (props.IsXButton2Pressed) // Mouse Forward button
            {
                if (AppNavView?.Frame?.CanGoForward == true)
                {
                    AppNavView?.Frame?.GoForward();
                    e.Handled = true;
                }
            }
        }

        private void MainWindow_Activated(object sender, WindowActivatedEventArgs args)
        {
            UpdateControlsVisibility();
        }

        private readonly SemaphoreSlim _manyDeletesQueueSemaphore = new(1, 1);

        // Tracks the soft-limit dialog's open state and aggregated data for merging new notifications.
        private bool _isSoftLimitDialogOpen;
        private bool _forceCloseSoftLimitForHardLimit;
        private int _softLimitAggregatedNbFiles;
        private ManyDeletesInfo _currentSoftLimitInfo;

        public async Task ProcessManyDeleteQueue()
        {
            if (!_mainContentReady)
                return; // MainContent_Loaded will retry once XamlRoot is ready

            // Merge any newly queued soft-limit notifications directly into the currently open
            // soft-limit dialog instead of waiting for it to close before showing a new one.
            MergeQueuedSoftLimitsIntoOpenDialog();

            // A hard limit for the same sync takes priority over an open soft-limit dialog: close it.
            if (_isSoftLimitDialogOpen && ViewModel.ManyDeletesQueue.Any(i => i.NotificationType == TooManyDeletesNotificationType.HardLimit && i.SyncDbId == _currentSoftLimitInfo.SyncDbId))
            {
                _forceCloseSoftLimitForHardLimit = true;
                SoftLimitManyDeleteDialog.Hide();
            }

            if (!await _manyDeletesQueueSemaphore.WaitAsync(0))
                return; // Another call is already draining the queue

            try
            {
                while (ViewModel.ManyDeletesQueue.Count > 0)
                {
                    // Another ContentDialog may already be open on this XamlRoot (e.g. a conflict
                    // or account-removal dialog); only one can be shown at a time. If so, retry
                    // once it closes instead of showing ours now.
                    if (IsAnotherDialogOpen(out var openPopups))
                    {
                        RetryOnceClosed(openPopups);
                        return;
                    }

                    // Dequeue before showing so a re-entrant MergeQueuedSoftLimitsIntoOpenDialog()
                    // call can't merge/count this item again while its dialog is open.
                    ManyDeletesInfo manyDeletesInfo = ViewModel.ManyDeletesQueue.Dequeue();

                    bool shown = manyDeletesInfo.NotificationType == TooManyDeletesNotificationType.HardLimit
                        ? await ShowHardLimitManyDeleteDialogue(manyDeletesInfo)
                        : await ShowSoftLimitManyDeleteDialogue(manyDeletesInfo);

                    if (!shown)
                    {
                        RequeueManyDeletesInfo(manyDeletesInfo); // Retry later (e.g. once XamlRoot is ready)
                        return;
                    }

                    // More soft-limit notifications may have been queued while the dialog we just
                    // showed was open; merge them now before looping to the next item.
                    MergeQueuedSoftLimitsIntoOpenDialog();
                }
            }
            finally
            {
                _manyDeletesQueueSemaphore.Release();
            }
        }

        // Puts an item back at the front of the queue (Queue<T> has no built-in way to do this).
        private void RequeueManyDeletesInfo(ManyDeletesInfo info)
        {
            var remaining = ViewModel.ManyDeletesQueue.ToArray();
            ViewModel.ManyDeletesQueue.Clear();
            ViewModel.ManyDeletesQueue.Enqueue(info);
            foreach (var item in remaining)
                ViewModel.ManyDeletesQueue.Enqueue(item);
        }

        // Merges consecutive soft-limit items for the same sync into the currently open dialog.
        // Items for other syncs stay queued and are shown once this dialog closes.
        private void MergeQueuedSoftLimitsIntoOpenDialog()
        {
            if (!_isSoftLimitDialogOpen)
                return;

            while (ViewModel.ManyDeletesQueue.Count > 0
                && ViewModel.ManyDeletesQueue.Peek().NotificationType == TooManyDeletesNotificationType.SoftLimit
                && ViewModel.ManyDeletesQueue.Peek().SyncDbId == _currentSoftLimitInfo.SyncDbId)
            {
                ManyDeletesInfo info = ViewModel.ManyDeletesQueue.Dequeue();
                _softLimitAggregatedNbFiles += info.NbFiles;
                _currentSoftLimitInfo = info;
                ManyDeletesNbFiles = _softLimitAggregatedNbFiles;
            }
        }

        private bool IsAnotherDialogOpen(out IReadOnlyList<Popup> openPopups)
        {
            openPopups = this.Content?.XamlRoot is XamlRoot xamlRoot
                ? VisualTreeHelper.GetOpenPopupsForXamlRoot(xamlRoot)
                : Array.Empty<Popup>();
            return openPopups.Count > 0;
        }

        // Re-attempts ProcessManyDeleteQueue() as soon as one of the given popups closes, instead
        // of blocking/polling while waiting.
        private void RetryOnceClosed(IReadOnlyList<Popup> openPopups)
        {
            async void OnClosed(object? s, object e)
            {
                foreach (var p in openPopups)
                    p.Closed -= OnClosed;

                await ProcessManyDeleteQueue();
            }

            foreach (var popup in openPopups)
                popup.Closed += OnClosed;
        }

        private void MainWindow_Closed(object sender, WindowEventArgs args)
        {
            if ((App.Current as App)?.CurrentWindow == this)
            {
                args.Handled = true;
                this.Hide();
                return;
            }

            ViewModel.PropertyChanged -= ViewModel_PropertyChanged;
            Closed -= MainWindow_Closed;
            Activated -= MainWindow_Activated;
            this.Content.PointerPressed -= OnPointerPressed;
        }

        private void ViewModel_PropertyChanged(object? sender, PropertyChangedEventArgs e)
        {
            if (e.PropertyName == nameof(AppModel.IsInitialized) || e.PropertyName == nameof(AppModel.UpdateRequired))
                UpdateControlsVisibility();
        }

        private void UpdateControlsVisibility()
        {
            if (NavView is null || SplashScreen is null) return;
            if (!ViewModel.IsInitialized)
            {
                SplashScreen.Visibility = Visibility.Visible;

                NavView.Visibility = Visibility.Collapsed;
                UpdateRequiredControl.Visibility = Visibility.Collapsed;
            }
            else if (ViewModel.UpdateRequired)
            {
                UpdateRequiredControl.Visibility = Visibility.Visible;

                SplashScreen.Visibility = Visibility.Collapsed;
                NavView.Visibility = Visibility.Collapsed;
            }
            else
            {
                NavView.Visibility = Visibility.Visible;

                SplashScreen.Visibility = Visibility.Collapsed;
                UpdateRequiredControl.Visibility = Visibility.Collapsed;
            }
        }

        // ProcessManyDeleteQueue() already checks IsAnotherDialogOpen() before calling this, but we
        // still guard ShowAsync() itself in case another dialog opens in between (race condition).
        private async Task<ContentDialogResult?> TryShowDialogAsync(ContentDialog dialog)
        {
            if (this.Content?.XamlRoot is null)
                return null;

            try
            {
                return await dialog.ShowAsync();
            }
            catch (Exception ex)
            {
                Logger.Log(Logger.Level.Warning, $"Failed to show ContentDialog: {ex}");
                return null;
            }
        }

        // Prevents the dialog from being light-dismissed (e.g. clicking outside or pressing Escape).
        private void PreventDialogLightDismiss(ContentDialog sender, ContentDialogClosingEventArgs e)
        {
            if (e.Result == ContentDialogResult.None)
                e.Cancel = true;
        }

        // Same as PreventDialogLightDismiss, but allows the dialog to be closed programmatically
        // (via Hide()) when a hard-limit notification needs to take priority over it.
        private void PreventSoftLimitDialogLightDismiss(ContentDialog sender, ContentDialogClosingEventArgs e)
        {
            if (e.Result == ContentDialogResult.None && !_forceCloseSoftLimitForHardLimit)
                e.Cancel = true;
        }

        public async Task<bool> ShowHardLimitManyDeleteDialogue(ManyDeletesInfo manyDeletesInfo)
        {
            // XamlRoot must be set in the case of a ContentDialog running in a Desktop app
            HardLimitManyDeleteDialog.XamlRoot = this.Content.XamlRoot;
            ManyDeletesNbFiles = manyDeletesInfo.NbFiles;

            Utility.BringCurrentWindowToFront();
            var result = await TryShowDialogAsync(HardLimitManyDeleteDialog);
            if (result is null)
                return false;

            TooManyDeletesUserChoice userChoice = result switch
            {
                ContentDialogResult.Primary => TooManyDeletesUserChoice.Revert,
                ContentDialogResult.Secondary => TooManyDeletesUserChoice.Continue,
                _ => TooManyDeletesUserChoice.Revert
            };

            await App.ServiceProvider.GetRequiredService<IServerCommService>().AcknowledgeManyDeletes(manyDeletesInfo.SyncDbId, userChoice, CancellationToken.None);
            return true;
        }

        public async Task<bool> ShowSoftLimitManyDeleteDialogue(ManyDeletesInfo manyDeletesInfo)
        {
            // XamlRoot must be set in the case of a ContentDialog running in a Desktop app
            SoftLimitManyDeleteDialog.XamlRoot = this.Content.XamlRoot;

            _softLimitAggregatedNbFiles = manyDeletesInfo.NbFiles;
            _currentSoftLimitInfo = manyDeletesInfo;
            ManyDeletesNbFiles = _softLimitAggregatedNbFiles;
            SoftLimitDoNotShowAgainCheckBox.IsChecked = !ViewModel.Settings.AskBeforeDelete;

            Utility.BringCurrentWindowToFront();

            _isSoftLimitDialogOpen = true;
            ContentDialogResult? result;
            try
            {
                result = await TryShowDialogAsync(SoftLimitManyDeleteDialog);
            }
            finally
            {
                _isSoftLimitDialogOpen = false;
            }

            // The dialog was force-closed to give priority to a hard-limit notification; skip the
            // usual post-processing (settings update / trash link) and let the hard limit be shown.
            if (_forceCloseSoftLimitForHardLimit)
            {
                _forceCloseSoftLimitForHardLimit = false;
                return true;
            }

            if (result is null)
                return false;

            bool doNotShowAgain = SoftLimitDoNotShowAgainCheckBox.IsChecked ?? false;
            await ViewModel.Settings.ChangeNotifyBeforeDelete(!doNotShowAgain);

            if (result == ContentDialogResult.Secondary)
            {
                Sync? sync = ViewModel.AllSyncs.FirstOrDefault(s => s.DbId == _currentSoftLimitInfo.SyncDbId);
                Uri? trashUrl = sync?.Drive.GetWebTrashUri();
                if (trashUrl != null)
                {
                    Logger.Log(Logger.Level.Debug, $"ShowSoftLimitManyDeleteDialogue: Launching trash URL: {trashUrl}");
                    await Windows.System.Launcher.LaunchUriAsync(trashUrl);
                }
                else
                {
                    Logger.Log(Logger.Level.Error, $"ShowSoftLimitManyDeleteDialogue: Unable to get trash URL for sync with DbId {_currentSoftLimitInfo.SyncDbId}.");
                }
            }
            return true;
        }

        private void AppTitleBar_BackRequested(TitleBar sender, object args)
        {
            if (AppNavView?.Frame?.CanGoBack is null)
            {
                Logger.Log(Logger.Level.Warning, "BackRequested event triggered but AppNavView or its Frame is null. Cannot navigate back.");
                return;
            }

            if (AppNavView.Frame.CanGoBack)
                AppNavView.Frame.GoBack();
        }
    }
}
