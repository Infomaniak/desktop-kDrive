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

namespace Infomaniak.kDrive.Pages
{
    public sealed partial class MaintenanceErrorPage : SpecialErroBasePage
    {
        private readonly IAnalyticsService _analyticsService = App.ServiceProvider.GetRequiredService<IAnalyticsService>();

        public MaintenanceErrorPage() : base([SyncErrorStates.Maintenance])
        {
            Logger.Log(Logger.Level.Info, "Navigated to MaintenanceErrorPage - Initializing MaintenanceErrorPage components");
            InitializeComponent();
            Logger.Log(Logger.Level.Debug, "MaintenanceErrorPage components initialized");
        }

        protected override void OnNavigatedTo(NavigationEventArgs e)
        {
            base.OnNavigatedTo(e);
            _analyticsService.TrackPageView(Analytics.Keys.Category.MaintenanceErrorPage);
        }
        private async void RetryButton_Click(object sender, RoutedEventArgs e)
        {
            Logger.Log(Logger.Level.Info, "Retry button clicked - Starting sync");
            _analyticsService.TrackClick(Analytics.Keys.Category.MaintenanceErrorPage, Analytics.Keys.EventName.StartSync);
            await RestartSync();
        }
    }
}
