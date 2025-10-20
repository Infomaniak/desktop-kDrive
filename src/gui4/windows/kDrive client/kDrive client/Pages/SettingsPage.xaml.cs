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

using Infomaniak.kDrive.Converters;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Data;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Navigation;
using Microsoft.VisualBasic.Devices;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.Networking.Connectivity;
using static System.Runtime.InteropServices.JavaScript.JSType;

namespace Infomaniak.kDrive.Pages
{
    public sealed partial class SettingsPage : Page
    {
        private AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        public AppModel ViewModel => _viewModel;
        public SettingsPage()
        {
            Logger.Log(Logger.Level.Info, "Navigated to SettingsPage - Initializing SettingsPage components");
            InitializeComponent();
            Logger.Log(Logger.Level.Debug, "SettingsPage components initialized");
        }

        private void SyncUpToDateHyperlinkButton_Click(object sender, RoutedEventArgs e)
        {
            ((App)Application.Current).CurrentWindow?.AppWindow.Hide();
        }

        private void SyncInProgressHyperlinkButton_Click(object sender, RoutedEventArgs e)
        {
            Frame.Navigate(typeof(ActivityPage));
        }

        private void SyncInPauseHyperlinkButton_Click(object sender, RoutedEventArgs e)
        {
            if(ViewModel.SelectedSync == null)
            {
                Logger.Log(Logger.Level.Warning, "No sync is selected, cannot resume sync.");
                return;
            }
            ViewModel.SelectedSync.SyncStatus = SyncStatus.Running; // Todo: Replace with actual resume logic
        }
        private void CreateAccountButton_Click(object sender, RoutedEventArgs e)
        {
            (App.Current as App)?.StartOnboarding();
        }

        private void TextBlockUpdateInfoLoaded(object sender, RoutedEventArgs e)
        {
            if(sender is TextBlock textBlock)
            {
                var resourceLoader = Windows.ApplicationModel.Resources.ResourceLoader.GetForViewIndependentUse();
                var updateData = ViewModel.Settings.UpdateManager.AvailableUpdate;
                if( updateData == null )
                {
                    textBlock.Text = string.Format(resourceLoader.GetString("Page_SettingsPage_UpdateDetails/Text"), "?", "?", "?", "?");
                    return;
                }
                DateTime date;
                if(DateTime.TryParseExact(updateData.BuildVersion, "yyyyMMdd", CultureInfo.InvariantCulture, DateTimeStyles.None, out DateTime parsedDate))
                {
                    date = parsedDate;
                }
                else
                {
                    Logger.Log(Logger.Level.Warning, "DateToStringConverter: string value is not in the expected format 'yyyyMMdd'.");
                    date = DateTime.MinValue;
                }
                textBlock.Text = string.Format(resourceLoader.GetString("Page_SettingsPage_UpdateDetails/Text"), updateData.Tag, updateData.BuildVersion, date.ToString("D", CultureInfo.CurrentCulture), date.Year);

            }
        }
    }
}
