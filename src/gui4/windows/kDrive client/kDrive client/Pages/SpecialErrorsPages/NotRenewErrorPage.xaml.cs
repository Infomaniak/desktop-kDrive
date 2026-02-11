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

using Infomaniak.kDrive.Types;
using Microsoft.UI.Xaml;
using System;

namespace Infomaniak.kDrive.Pages
{
    public sealed partial class NotRenewErrorPage : SpecialErroBasePage
    {
        public NotRenewErrorPage() : base([SyncErrorStates.NotRenew])
        {
            Logger.Log(Logger.Level.Info, "Navigated to NotRenewErrorPage - Initializing NotRenewErrorPage components");
            InitializeComponent();
            UpdateContent();
            Logger.Log(Logger.Level.Debug, "NotRenewErrorPage components initialized");
        }

        private void UpdateContent()
        {
            TitleTextBlock.Text = Localizer.Localizer.GetString("driveLockedErrorTitle");

            if (ViewModel.SelectedSync?.Drive.IsAdmin ?? false)
            {
                SubtitleTextBlock.Text = Localizer.Localizer.GetString("driveLockedAdminErrorDescription");
                MainButton.Content = Localizer.Localizer.GetString("buttonUpdateSubscription");
                SecondaryButton.Content = Localizer.Localizer.GetString("buttonRefresh");
                SecondaryButton.Visibility = Visibility.Visible;
            }
            else
            {
                SubtitleTextBlock.Text = Localizer.Localizer.GetString("driveLockedAdminErrorDescription");
                MainButton.Content = Localizer.Localizer.GetString("buttonRefresh");
                SecondaryButton.Visibility = Visibility.Collapsed;
            }
        }

        private async void SecondaryButton_Click(object sender, RoutedEventArgs e)
        {
            Logger.Log(Logger.Level.Info, "Renew drive button clicked - Opening drive renewal page");
            await RestartSync();
        }

        private async void MainButton_Click(object sender, RoutedEventArgs e)
        {
            if (ViewModel.SelectedSync?.Drive.IsAdmin ?? false)
            {
                Logger.Log(Logger.Level.Info, "Renew drive button clicked - Opening drive renewal page");
                await Windows.System.Launcher.LaunchUriAsync(App.Constants.Drive.RenewUrl(ViewModel.SelectedSync?.Drive.DriveId));
            }
            else
            {
                Logger.Log(Logger.Level.Info, "Retry button clicked - Starting sync to retry connection");
                await RestartSync();
            }
        }
    }
}
