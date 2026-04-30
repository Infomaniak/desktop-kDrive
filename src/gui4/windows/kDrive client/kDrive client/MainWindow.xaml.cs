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
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Input;
using System;
using System.ComponentModel;

namespace Infomaniak.kDrive
{
    public sealed partial class MainWindow : Window
    {
        public AppNavigationView AppNavView { get { return NavView; } }
        public AppModel ViewModel { get; } = App.ServiceProvider.GetRequiredService<AppModel>();

        public MainWindow()
        {
            InitializeComponent();
            this.ExtendsContentIntoTitleBar = true;  // enable custom titlebar
            this.SetTitleBar(AppTitleBar);
            Utility.SetWindowProperties(this, 900, 600, true);
            Utility.SetWindowCurrentSize(this, 1025, 683); // Set to the minimum size keeping the nav bar panel open by default
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

        private void MainWindow_Activated(object sender, WindowActivatedEventArgs args)
        {
            UpdateControlsVisibility();
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
