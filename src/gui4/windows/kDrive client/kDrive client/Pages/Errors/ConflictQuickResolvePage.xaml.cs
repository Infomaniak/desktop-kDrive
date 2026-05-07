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
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Navigation;
using System;
using static Infomaniak.kDrive.ViewModels.AppModel;

namespace Infomaniak.kDrive.Pages.Errors
{
    public sealed partial class ConflictQuickResolvePage : Page
    {
        private static readonly IAnalyticsService _analyticsService = App.ServiceProvider.GetRequiredService<IAnalyticsService>();
        private AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        public AppModel ViewModel { get { return _viewModel; } }
        private ErrorPageVM? _errorPageVM;

        public ConflictQuickResolvePage()
        {
            Logger.Log(Logger.Level.Info, "Navigated to ConflictQuickResolvePage - Initializing components");
            InitializeComponent();
            Logger.Log(Logger.Level.Debug, "ConflictQuickResolvePage components initialized");
        }

        protected override async void OnNavigatedTo(NavigationEventArgs e)
        {
            _errorPageVM = new ErrorPageVM();
            _analyticsService.TrackPageView(Analytics.Keys.Category.BatchConflictResolutionPage);
        }

        protected override void OnNavigatedFrom(NavigationEventArgs e)
        {
            if (_errorPageVM is not null)
                _errorPageVM.Dispose();
        }

        private void BreadcrumbError_Click(object sender, object e)
        {
            Logger.Log(Logger.Level.Debug, "Navigating to ActivityPage");
            Frame.Navigate(typeof(ErrorPage));
            _analyticsService.TrackClick(Analytics.Keys.Category.BatchConflictResolutionPage, Analytics.Keys.EventName.ErrorBreadcrumb);
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
        private void RadioButton_Loaded(object sender, RoutedEventArgs e)
        {
            if (sender is RadioButton radioButton)
            {
                var glyphGrid = FindChild<Grid>(radioButton, "OuterEllipse");
                if (glyphGrid is not null)
                    glyphGrid.VerticalAlignment = VerticalAlignment.Center;
            }
        }

        private static T? FindChild<T>(DependencyObject parent, string childNameInside) where T : FrameworkElement
        {
            for (var i = 0; i < VisualTreeHelper.GetChildrenCount(parent); i++)
            {
                var child = VisualTreeHelper.GetChild(parent, i);
                if (child is FrameworkElement fe && fe.Name == childNameInside)
                    return fe.Parent as T;

                var result = FindChild<T>(child, childNameInside);
                if (result is not null)
                    return result;
            }
            return null;
        }

        private async void ApplyButton_Click(object sender, RoutedEventArgs e)
        {
            ConflictResolutionStrategy resolutionStrategy;
            if (KeepMostRecentRadioButton.IsChecked == true)
            {
                resolutionStrategy = ConflictResolutionStrategy.KeepMostRecent;
            }
            else if (KeepLocalRadioButton.IsChecked == true)
            {
                resolutionStrategy = ConflictResolutionStrategy.KeepLocal;
            }
            else if (KeepRemoteRadioButton.IsChecked == true)
            {
                resolutionStrategy = ConflictResolutionStrategy.KeepRemote;
            }
            else
            {
                Logger.Log(Logger.Level.Warning, "Apply button clicked without a resolution strategy selected. No action will be taken.");
                return;
            }

            if (_errorPageVM is null)
            {
                Logger.Log(Logger.Level.Error, "_errorPageVM is null when Apply button clicked. This should never happen, but if it does, we log the error and re-enable the buttons to allow the user to try again or choose to manage conflicts individually.");
                Utility.ShowUnexpectedErrorTeachingTip();
                return;
            }

            ApplyButton.IsEnabled = false;
            ManageIndividuallyButton.IsEnabled = false;

            if (_errorPageVM.Sync is null)
            {
                Logger.Log(Logger.Level.Error, "_errorPageVM.Sync is null when Apply button clicked. This should never happen, but if it does, we log the error and re-enable the buttons to allow the user to try again or choose to manage conflicts individually.");
                Utility.ShowUnexpectedErrorTeachingTip();
                ApplyButton.IsEnabled = true;
                ManageIndividuallyButton.IsEnabled = true;
                return;
            }

            if (!await _errorPageVM.Sync.SolveConflictsQuick(resolutionStrategy))
            {
                Logger.Log(Logger.Level.Error, "Failed to resolve conflicts quickly. Re-enabling buttons to allow user to try again or manage individually.");
                Utility.ShowUnexpectedErrorTeachingTip();
            }
            else
            {
                _analyticsService.TrackClick(Analytics.Keys.Category.BatchConflictResolutionPage, Analytics.Keys.EventName.Apply);
                Frame.Navigate(typeof(ActivityPage));
            }
            ApplyButton.IsEnabled = true;
            ManageIndividuallyButton.IsEnabled = true;
        }

        private void ManageIndividuallyButton_Click(object sender, RoutedEventArgs e)
        {
            _analyticsService.TrackClick(Analytics.Keys.Category.BatchConflictResolutionPage, Analytics.Keys.EventName.OpenIndividualResolution);
            Frame.Navigate(typeof(ResolveManyConflictPage));
        }

        private void KeepMostRecentRadioButton_Checked(object sender, RoutedEventArgs e)
        {
            if (!IsLoaded)
                return;
            _analyticsService.TrackClick(Analytics.Keys.Category.BatchConflictResolutionPage, Analytics.Keys.EventName.KeepMostRecent);
        }

        private void KeepRemoteRadioButton_Checked(object sender, RoutedEventArgs e)
        {
            if (!IsLoaded)
                return;
            _analyticsService.TrackClick(Analytics.Keys.Category.BatchConflictResolutionPage, Analytics.Keys.EventName.KeepOnline);
        }

        private void KeepLocalRadioButton_Checked(object sender, RoutedEventArgs e)
        {
            if (!IsLoaded)
                return;
            _analyticsService.TrackClick(Analytics.Keys.Category.BatchConflictResolutionPage, Analytics.Keys.EventName.KeepRemote);
        }
    }
}