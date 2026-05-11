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

using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System;
using System.Collections.Generic;
using System.Net.Http;
using System.Text.RegularExpressions;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.Pages.Settings
{
    public sealed partial class UpdateDialogPage : Page
    {
        private readonly AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        public AppModel ViewModel => _viewModel;

        public UpdateDialogPage()
        {
            Logger.Log(Logger.Level.Info, "Navigated to UpdateDialogPage - Initializing UpdateDialogPage components");
            InitializeComponent();
            Logger.Log(Logger.Level.Debug, "UpdateDialogPage components initialized");
            Loaded += OnLoaded;
        }

        private async void OnLoaded(object sender, RoutedEventArgs e)
        {
            await LoadReleaseNotesAsync();
        }

        private async Task LoadReleaseNotesAsync()
        {
            var availableUpdate = ViewModel.Settings?.UpdateManager?.AvailableUpdate;
            if (availableUpdate is null)
            {
                ReleaseNotesLoader.IsLoading = false;
                return;
            }

            using var httpClient = new HttpClient();
            string? releaseNotes = null;
            try
            {
                releaseNotes = await httpClient.GetStringAsync(availableUpdate.ChangeLogUrlLocalized);
            }
            catch (Exception ex)
            {
                try
                {
                    Logger.Log(Logger.Level.Info, $"Failed to load release notes from localized URL: {ex.Message}. Attempting default language URL.");
                    releaseNotes = await httpClient.GetStringAsync(availableUpdate.ChangeLogUrlDefaultLanguage);
                }
                catch (Exception innerEx)
                {
                    Logger.Log(Logger.Level.Warning, $"Failed to load release notes from default language URL: {innerEx.Message}");
                    ReleaseNotesLoader.IsLoading = false;
                    return;
                }
            }

            var items = ParseListItems(releaseNotes);
            ReleaseNotesItemsControl.ItemsSource = items;
            ReleaseNotesLoader.IsLoading = false;
        }

        private static List<string> ParseListItems(string html)
        {
            var results = new List<string>();
            foreach (Match match in Regex.Matches(html, @"<li[^>]*>(.*?)</li>", RegexOptions.Singleline | RegexOptions.IgnoreCase))
            {
                // Strip any remaining HTML tags and trim whitespace
                var text = Regex.Replace(match.Groups[1].Value, @"<[^>]+>", string.Empty).Trim();
                if (!string.IsNullOrEmpty(text))
                    results.Add(text);
            }
            return results;
        }

        private void RemindLaterButton_Click(object sender, RoutedEventArgs e)
        {
            Logger.Log(Logger.Level.Info, "User clicked 'Remind me later' on UpdateDialogPage.");
            (App.Current as App)?.CloseUpdateWindow();
        }

        private async void InstallNowButton_Click(object sender, RoutedEventArgs e)
        {
            if (sender is Button btn)
                btn.IsEnabled = false;

            Logger.Log(Logger.Level.Info, "User clicked 'Install now' on UpdateDialogPage, starting update process.");

            if (!await UpdateManager.StartUpdate())
            {
                Logger.Log(Logger.Level.Error, "Update process failed to start.");
                Utility.ShowUnexpectedErrorTeachingTip();
            }

            await Task.Delay(5000);

            if (sender is Button button)
                button.IsEnabled = true;
        }

        private async void IgnoreVersionButton_Click(object sender, RoutedEventArgs e)
        {
            Logger.Log(Logger.Level.Info, "User clicked 'Ignore this version' on UpdateDialogPage.");

            if (ViewModel.Settings?.UpdateManager is null || !await ViewModel.Settings.UpdateManager.SkipVersion())
            {
                Logger.Log(Logger.Level.Error, "Update process failed to skip update.");
                Utility.ShowUnexpectedErrorTeachingTip();
                return;
            }

            (App.Current as App)?.CloseUpdateWindow();
        }
    }
}
