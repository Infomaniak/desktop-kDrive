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
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Navigation;
using System;

namespace Infomaniak.kDrive.Pages
{
    public sealed partial class NotRenewErrorPage : SpecialErroBasePage
    {
        private readonly IAnalyticsService _analyticsService = App.ServiceProvider.GetRequiredService<IAnalyticsService>();
        public NotRenewErrorPage() : base([SyncErrorStates.NotRenew])
        {
            Logger.LogInfo("Navigated to NotRenewErrorPage - Initializing NotRenewErrorPage components");
            InitializeComponent();
            UpdateContent();
            Logger.LogDebug("NotRenewErrorPage components initialized");
        }

        protected override void OnNavigatedTo(NavigationEventArgs e)
        {
            base.OnNavigatedTo(e);
            _analyticsService.TrackPageView(Analytics.Keys.Category.NotRenewErrorPage);
        }

        private void UpdateContent()
        {
            TitleTextBlock.Text = Localizer.Instance.GetString("driveLockedErrorTitle");

            if (ViewModel.SelectedSync?.Drive.IsAdmin ?? false)
            {
                SubtitleTextBlock.Text = Localizer.Instance.GetString("driveLockedAdminErrorDescription");
                MainButton.Content = Localizer.Instance.GetString("buttonUpdateSubscription");
                SecondaryButton.Content = Localizer.Instance.GetString("buttonRefresh");
                SecondaryButton.Visibility = Visibility.Visible;
            }
            else
            {
                SubtitleTextBlock.Text = Localizer.Instance.GetString("driveLockedAdminErrorDescription");
                MainButton.Content = Localizer.Instance.GetString("buttonRefresh");
                SecondaryButton.Visibility = Visibility.Collapsed;
            }
        }

        private async void SecondaryButton_Click(object sender, RoutedEventArgs e)
        {
            Logger.LogInfo("Renew drive button clicked - Restarting sync");
            _analyticsService.TrackClick(Analytics.Keys.Category.NotRenewErrorPage, Analytics.Keys.EventName.StartSync);
            await RestartSync();
        }

        private async void MainButton_Click(object sender, RoutedEventArgs e)
        {
            if (ViewModel.SelectedSync?.Drive.IsAdmin ?? false)
            {
                Logger.LogInfo("Renew drive button clicked - Opening drive renewal page");
                _analyticsService.TrackClick(Analytics.Keys.Category.NotRenewErrorPage, Analytics.Keys.EventName.OpenRenewWeb);

                var uriStore = App.ServiceProvider.GetRequiredService<IUriStore>();
                if (ViewModel.SelectedSync is null)
                {
                    Logger.LogError("SelectedSync is null in NotRenewErrorPage, cannot open renew link");
                    Utility.ShowUnexpectedErrorTeachingTip();
                    return;
                }

                if (!await uriStore.OpenInBrowserAsync(uriStore.RenewDriveUrl(ViewModel.SelectedSync.Drive)))
                {
                    Logger.LogWarning("Failed to open RenewDriveUrl repository URL in browser");
                    Utility.ShowUnexpectedErrorTeachingTip();
                }
            }
            else
            {
                Logger.LogInfo("Retry button clicked - Starting sync to retry connection");
                _analyticsService.TrackClick(Analytics.Keys.Category.NotRenewErrorPage, Analytics.Keys.EventName.StartSync);
                await RestartSync();
            }
        }
    }
}
