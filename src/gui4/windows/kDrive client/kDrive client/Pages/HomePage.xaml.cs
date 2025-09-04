/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
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

using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Data;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Foundation;
using Windows.Foundation.Collections;
using KDrive.ViewModels;

namespace KDrive
{
    public sealed partial class HomePage : Page
    {
        public AppModel _viewModel = ((App)Application.Current).Data;
        public AppModel ViewModel => _viewModel;
        public HomePage()
        {
            Logger.Log(Logger.Level.Info, "Navigated to HomePage - Initializing HomePage components");
            InitializeComponent();
            Logger.Log(Logger.Level.Debug, "HomePage components initialized");
        }

        private async void TrashButton_Click(object sender, RoutedEventArgs e)
        {
            Uri? trashUrl = ViewModel.SelectedSync?.Drive.GetWebTrashUri();
            if (trashUrl != null)
            {
                Logger.Log(Logger.Level.Debug, $"TrashButton_Click: Launching trash URL: {trashUrl}");
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
                await Windows.System.Launcher.LaunchUriAsync(sharedUrl);
            }
            else
            {
                Logger.Log(Logger.Level.Error, "No sync selected or unable to get shared URL.");
            }
        }
    }
}
