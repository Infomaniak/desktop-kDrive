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
using System;

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class QuickAccess : UserControl
    {
        private readonly IAnalyticsService _analyticsService = App.ServiceProvider.GetRequiredService<IAnalyticsService>();
        private readonly AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        public AppModel ViewModel => _viewModel;

        public QuickAccess()
        {
            InitializeComponent();
        }

        private async void TrashButton_Click(object sender, RoutedEventArgs e)
        {
            Uri? trashUrl = ViewModel.SelectedSync?.Drive.GetWebTrashUri();
            if (trashUrl != null)
            {
                Logger.Log(Logger.Level.Debug, $"TrashButton_Click: Launching trash URL: {trashUrl}");
                _analyticsService.TrackClick(Analytics.Keys.Category.HomePage, Analytics.Keys.EventName.OpenTrashWeb);
                await Windows.System.Launcher.LaunchUriAsync(trashUrl);
            }
            else
            {
                Logger.Log(Logger.Level.Error, "No sync selected or unable to get trash URL.");
            }
        }

        private async void FavoritesButton_Click(object sender, RoutedEventArgs e)
        {
            Uri? favoritesUrl = ViewModel.SelectedSync?.Drive.GetWebFavoritesUri();
            if (favoritesUrl != null)
            {
                Logger.Log(Logger.Level.Debug, $"Launching favorites URL: {favoritesUrl}"); 
                _analyticsService.TrackClick(Analytics.Keys.Category.HomePage, Analytics.Keys.EventName.OpenFavoritesWeb);
                await Windows.System.Launcher.LaunchUriAsync(favoritesUrl);
            }
            else
            {
                Logger.Log(Logger.Level.Error, "No sync selected or unable to get favorites URL.");
            }

        }

        private async void SharedButton_Click(object sender, RoutedEventArgs e)
        {
            Uri? sharedUrl = ViewModel.SelectedSync?.Drive.GetWebSharedUri();
            if (sharedUrl != null)
            {
                Logger.Log(Logger.Level.Debug, $"Launching shared URL: {sharedUrl}");
                _analyticsService.TrackClick(Analytics.Keys.Category.HomePage, Analytics.Keys.EventName.OpenSharedWeb);
                await Windows.System.Launcher.LaunchUriAsync(sharedUrl);
            }
            else
            {
                Logger.Log(Logger.Level.Error, "No sync selected or unable to get shared URL.");
            }
        }

        private async void OpenDriveOnlineButton_Click(object sender, RoutedEventArgs e)
        {
            Uri? onlineDriveUrl = ViewModel.SelectedSync?.Drive.GetWebUri();
            if (onlineDriveUrl != null)
            {
                Logger.Log(Logger.Level.Debug, $"Launching URL: {onlineDriveUrl}");
                _analyticsService.TrackClick(Analytics.Keys.Category.HomePage, Analytics.Keys.EventName.OpenKDriveWeb);
                await Windows.System.Launcher.LaunchUriAsync(onlineDriveUrl);
            }
            else
            {
                Logger.Log(Logger.Level.Error, "No sync selected or unable to get URL.");
            }
        }
    }
}
