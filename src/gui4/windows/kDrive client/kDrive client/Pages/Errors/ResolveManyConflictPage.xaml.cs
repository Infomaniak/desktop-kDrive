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
using Infomaniak.kDrive.CustomControls.Errors;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.ComponentModel;
using System.Linq;
using static Infomaniak.kDrive.ViewModels.AppModel;

namespace Infomaniak.kDrive.Pages.Errors
{
    public sealed partial class ResolveManyConflictPage : Page
    {
        private static readonly IAnalyticsService _analyticsService = App.ServiceProvider.GetRequiredService<IAnalyticsService>();
        private AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        public AppModel ViewModel { get { return _viewModel; } }
        private ErrorPageVM? _errorPageVM;
        public ResolveManyConflictPage()
        {
            Logger.Log(Logger.Level.Info, "Navigated to ResolveManyConflictPage - Initializing components");
            InitializeComponent();
            Logger.Log(Logger.Level.Debug, "ResolveManyConflictPage components initialized");
        }

        protected override async void OnNavigatedTo(NavigationEventArgs e)
        {
            _errorPageVM = new ErrorPageVM();
            _errorPageVM.PropertyChanged += OnErrorPageVMPropertyChanged;
            _analyticsService.TrackPageView(Analytics.Keys.Category.IndividualConflictResolutionPage);
        }

        protected override void OnNavigatedFrom(NavigationEventArgs e)
        {
            DetachEventHandlers();
        }
        private void DetachEventHandlers()
        {
            if (_errorPageVM is not null)
            {
                _errorPageVM.PropertyChanged -= OnErrorPageVMPropertyChanged;
                _errorPageVM.Dispose();
            }
        }

        private void OnErrorPageVMPropertyChanged(object? sender, PropertyChangedEventArgs e)
        {
            if (e.PropertyName == nameof(ErrorPageVM.ConflictsCount) && _errorPageVM?.ConflictsCount == 0)
            {
                Logger.Log(Logger.Level.Info, "All conflicts resolved, navigating to ActivityPage");
                Frame.Navigate(typeof(ActivityPage));
            }
        }

        private void BreadcrumbConflict_Click(object sender, object e)
        {
            Logger.Log(Logger.Level.Debug, "Navigating to Conflict quick");
            Frame.Navigate(typeof(ConflictQuickResolvePage));
            _analyticsService.TrackClick(Analytics.Keys.Category.IndividualConflictResolutionPage, Analytics.Keys.EventName.BatchConflictResolutionBreadcrumb);
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

        private async void ResolveManyConflictButton_Click(object sender, RoutedEventArgs e)
        {
            if (_errorPageVM is null)
            {
                Logger.Log(Logger.Level.Error, "ErrorPageVM is null in ResolveManyConflictButton_Click");
                Utility.ShowUnexpectedErrorTeachingTip();
                return;
            }


            var xamlRoot = this.XamlRoot;
            if (xamlRoot is null)
            {
                return;
            }
            ContentDialog dialog = new ContentDialog
            {
                XamlRoot = xamlRoot,
                DefaultButton = ContentDialogButton.Primary,
                CloseButtonText = Localizer.Instance.GetString("buttonClose"),
            };

            // Reset conflict filter to populate the conflict with list with all the filter.
            _errorPageVM.ConflictFilterText = "";

            // Set the content first to allow the dialog to measure properly
            dialog.Content = new ConflictDialog(_errorPageVM.FilteredConflictErrors.ToList(), dialog) { XamlRoot = xamlRoot };

            // Apply the style to allow wider content
            dialog.Resources["ContentDialogMaxWidth"] = Application.Current.Resources["Infomaniak.Style.ContentDialog.MaxWidth"];
            dialog.Resources["ContentDialogMaxHeight"] = Application.Current.Resources["Infomaniak.Style.ContentDialog.MaxHeight"];

            _analyticsService.TrackClick(Analytics.Keys.Category.IndividualConflictResolutionPage, Analytics.Keys.EventName.StartChoices);
            _ = await dialog.ShowAsync();
            await Utility.VisualTreeDisposeUtility.DisposeItemsAsync(dialog);
        }

        private async void ResolveOneConflictButton_Click(object sender, RoutedEventArgs e)
        {
            var xamlRoot = this.XamlRoot;
            if (xamlRoot is null)
            {
                return;
            }
            ContentDialog dialog = new ContentDialog
            {
                XamlRoot = xamlRoot,
                DefaultButton = ContentDialogButton.Primary,
                CloseButtonText = Localizer.Instance.GetString("buttonClose"),
            };

            Control? control = sender as Control;
            if (control is null)
            {
                Logger.Log(Logger.Level.Error, "Control is null in ResolveOneConflictButton_Click");
                Utility.ShowUnexpectedErrorTeachingTip();
                return;
            }

            Error? error = control.DataContext as Error;
            if (error is null)
            {
                Logger.Log(Logger.Level.Error, "Error is null in ResolveOneConflictButton_Click");
                Utility.ShowUnexpectedErrorTeachingTip();
                return;
            }

            // Set the content first to allow the dialog to measure properly
            dialog.Content = new ConflictDialog(error, dialog) { XamlRoot = xamlRoot };

            // Apply the style to allow wider content
            dialog.Resources["ContentDialogMaxWidth"] = Application.Current.Resources["Infomaniak.Style.ContentDialog.MaxWidth"];
            dialog.Resources["ContentDialogMaxHeight"] = Application.Current.Resources["Infomaniak.Style.ContentDialog.MaxHeight"];
            _analyticsService.TrackClick(Analytics.Keys.Category.IndividualConflictResolutionPage, Analytics.Keys.EventName.ManageSingleConflict);
            _ = await dialog.ShowAsync();
            await Utility.VisualTreeDisposeUtility.DisposeItemsAsync(dialog);
        }

        private void AutoSuggestBox_TextChanged(AutoSuggestBox sender, AutoSuggestBoxTextChangedEventArgs args)
        {
            _analyticsService.TrackClick(Analytics.Keys.Category.IndividualConflictResolutionPage, Analytics.Keys.EventName.ValidateSearch);
        }
    }
}