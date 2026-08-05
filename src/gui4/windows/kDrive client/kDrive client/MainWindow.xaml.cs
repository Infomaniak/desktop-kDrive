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
using Microsoft.UI.Xaml.Input;
using System;
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

        private async void MainWindow_Activated(object sender, WindowActivatedEventArgs args)
        {
            UpdateControlsVisibility();
            await ProcessManyDeleteQueue();
        }

        private readonly SemaphoreSlim _manyDeletesQueueSemaphore = new(1, 1);

        public async Task ProcessManyDeleteQueue()
        {
            if (!await _manyDeletesQueueSemaphore.WaitAsync(0))
                return; // Another call is already draining the queue

            try
            {
                while (ViewModel.ManyDeletesQueue.Count > 0)
                {
                    ManyDeletesInfo manyDeletesInfo = ViewModel.ManyDeletesQueue.Dequeue();

                    if (manyDeletesInfo.NotificationType == TooManyDeletesNotificationType.HardLimit)
                        await ShowHardLimitManyDeleteDialogue(manyDeletesInfo);
                    else if (manyDeletesInfo.NotificationType == TooManyDeletesNotificationType.SoftLimit)
                        await ShowSoftLimitManyDeleteDialogue(manyDeletesInfo);
                }
            }
            finally
            {
                _manyDeletesQueueSemaphore.Release();
            }
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
        public async Task ShowHardLimitManyDeleteDialogue(ManyDeletesInfo manyDeletesInfo)
        {
            ContentDialog dialog = new ContentDialog();

            // XamlRoot must be set in the case of a ContentDialog running in a Desktop app
            dialog.XamlRoot = this.Content.XamlRoot;
            dialog.Title = Localizer.Instance.GetString1i("manyDeleteDialogTitle", manyDeletesInfo.NbFiles);
            dialog.PrimaryButtonText = Localizer.Instance.GetString("manyDeleteDialogHardLimitPrimary");
            dialog.SecondaryButtonText = Localizer.Instance.GetString("manyDeleteDialogHardLimitSecondary");
            dialog.DefaultButton = ContentDialogButton.Primary;
            dialog.Content = Localizer.Instance.GetString("manyDeleteDialogHardLimitContent");

            dialog.Closing += (s, e) =>
            {
                if (e.Result == ContentDialogResult.None)
                {
                    e.Cancel = true; // Prevent the dialog from closing
                }
            };

            Utility.BringCurrentWindowToFront();
            var result = await dialog.ShowAsync();

            TooManyDeletesUserChoice userChoice = result switch
            {
                ContentDialogResult.Primary => TooManyDeletesUserChoice.Revert,
                ContentDialogResult.Secondary => TooManyDeletesUserChoice.Continue,
                _ => TooManyDeletesUserChoice.Revert
            };

            await App.ServiceProvider.GetRequiredService<IServerCommService>().AcknowledgeManyDeletes(manyDeletesInfo.SyncDbId, userChoice, CancellationToken.None);
        }

        public async Task ShowSoftLimitManyDeleteDialogue(ManyDeletesInfo manyDeletesInfo)
        {
            ContentDialog dialog = new ContentDialog();

            // XamlRoot must be set in the case of a ContentDialog running in a Desktop app
            dialog.XamlRoot = this.Content.XamlRoot;
            dialog.Title = Localizer.Instance.GetString1i("manyDeleteDialogTitle", manyDeletesInfo.NbFiles);
            dialog.PrimaryButtonText = Localizer.Instance.GetString("buttonClose");
            dialog.SecondaryButtonText = Localizer.Instance.GetString("buttonOpenTrash");
            dialog.DefaultButton = ContentDialogButton.Primary;

            StackPanel contentPanel = new StackPanel
            {
                VerticalAlignment = VerticalAlignment.Stretch,
                HorizontalAlignment = HorizontalAlignment.Stretch,
                Spacing = (double)Application.Current.Resources["Infomaniak.Style.Spacing.M"]
            };
            contentPanel.Children.Add(new TextBlock
            {
                Text = Localizer.Instance.GetString("manyDeleteDialogSoftLimitContent"),
                TextWrapping = TextWrapping.Wrap,
            });
            CheckBox doNotShowAgainCheckBox = new CheckBox
            {
                Content = Localizer.Instance.GetString("manyDeleteDialogSoftLimitDoNotShowAgain"),
                IsChecked = !ViewModel.Settings.AskBeforeDelete
            };
            contentPanel.Children.Add(doNotShowAgainCheckBox);
            dialog.Content = contentPanel;

            dialog.Closing += (s, e) =>
            {
                if (e.Result == ContentDialogResult.None)
                {
                    e.Cancel = true; // Prevent the dialog from closing
                }
            };

            Utility.BringCurrentWindowToFront();
            var result = await dialog.ShowAsync();

            bool doNotShowAgain = doNotShowAgainCheckBox.IsChecked ?? false;
            await ViewModel.Settings.ChangeNotifyBeforeDelete(!doNotShowAgain);

            if (result == ContentDialogResult.Secondary)
            {
                Sync? sync = ViewModel.AllSyncs.FirstOrDefault(s => s.DbId == manyDeletesInfo.SyncDbId);
                Uri? trashUrl = sync?.Drive.GetWebTrashUri();
                if (trashUrl != null)
                {
                    Logger.Log(Logger.Level.Debug, $"ShowSoftLimitManyDeleteDialogue: Launching trash URL: {trashUrl}");
                    await Windows.System.Launcher.LaunchUriAsync(trashUrl);
                }
                else
                {
                    Logger.Log(Logger.Level.Error, $"ShowSoftLimitManyDeleteDialogue: Unable to get trash URL for sync with DbId {manyDeletesInfo.SyncDbId}.");
                }
            }
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
