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
using Infomaniak.kDrive.Analytics;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using System;
using static Infomaniak.kDrive.ViewModels.AppModel;

namespace Infomaniak.kDrive.Pages.Errors
{
    public sealed partial class ErrorPage : Page
    {
        private readonly IAnalyticsService _analyticsService = App.ServiceProvider.GetRequiredService<IAnalyticsService>();
        private readonly AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        public AppModel ViewModel { get { return _viewModel; } }
        private ErrorPageVM? _errorPageVM;
        private NavigationParameter _navigationParameter = NavigationParameter.Default;
        static private double _lastVerticalOffset = 0;
        public enum NavigationParameter
        {
            Default,
            LastVerticalOffset
        }

        public ErrorPage()
        {
            Logger.Log(Logger.Level.Info, "Navigated to ErrorPage - Initializing ErrorPage components");
            InitializeComponent();
            Logger.Log(Logger.Level.Debug, "ErrorPage components initialized");
            Loaded += ErrorPage_Loaded;
        }

        private void ErrorPage_Loaded(object sender, RoutedEventArgs e)
        {
            if (_navigationParameter == NavigationParameter.LastVerticalOffset)
            {
                ErrorScrollView.ScrollTo(0, _lastVerticalOffset, new ScrollingScrollOptions(ScrollingAnimationMode.Disabled, ScrollingSnapPointsMode.Ignore));
            }
        }

        protected override void OnNavigatedTo(NavigationEventArgs e)
        {
            _errorPageVM = new ErrorPageVM();
            _analyticsService.TrackPageView(Analytics.Keys.Category.ErrorPage);
            if (e.NavigationMode != NavigationMode.New)
            {
                _navigationParameter = NavigationParameter.LastVerticalOffset;
            }
        }

        protected override void OnNavigatedFrom(NavigationEventArgs e)
        {
            if (_errorPageVM is not null)
                _errorPageVM.Dispose();

            _lastVerticalOffset = ErrorScrollView.VerticalOffset;

        }

        private void BreadcrumbActivity_Click(object sender, object e)
        {
            Logger.Log(Logger.Level.Debug, "Navigating to ActivityPage");
            Frame.Navigate(typeof(ActivityPage));
            _analyticsService.TrackClick(Analytics.Keys.Category.ErrorPage, Analytics.Keys.EventName.ActivityBreadcrumb);
        }

        private void OnSelectedSyncChanged(object sender, SelectedSyncChangedEventArgs e)
        {
            if (e.NewValue is not null)
            {
                if (_errorPageVM is not null)
                    _errorPageVM.Sync = e.NewValue;
                else
                    _errorPageVM = new ErrorPageVM { Sync = e.NewValue };
            }
        }

        private void ManyConflicts_ActionClick(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            _analyticsService.TrackClick(Analytics.Keys.Category.Errors, Analytics.Keys.EventName.ManageMultipleConflicts);
            Frame.Navigate(typeof(ConflictQuickResolvePage));
        }

        private void NoErrorHyperlinkButton_Click(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            var frame = Utility.GetFrame(this);
            if (frame is null)
            {
                Logger.Log(Logger.Level.Warning, "Failed to fetch current frame.");
                return;
            }

            frame.Navigate(typeof(Pages.ActivityPage));
            _analyticsService.TrackClick(Analytics.Keys.Category.ErrorPage, Analytics.Keys.EventName.OpenActivity);
        }

        private async void RefreshButton_Click(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            if (_errorPageVM is not null)
            {
                Control? control = sender as Control;
                if (control is not null)
                    control.IsEnabled = false;
                _analyticsService.TrackClick(Analytics.Keys.Category.ErrorPage, Analytics.Keys.EventName.RefreshErrors);
                await _errorPageVM.RefreshErrors();

                if (control is not null)
                    control.IsEnabled = true;
            }
        }
    }
}