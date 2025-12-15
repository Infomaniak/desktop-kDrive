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
using Microsoft.UI.Xaml.Controls;
using System;
using System.ComponentModel;

namespace Infomaniak.kDrive.Pages
{
    public sealed partial class DriveAccessDeniedPage : SpecialErroBasePage
    {
        public DriveAccessDeniedPage() : base([SyncErrorStates.AccessDenied])
        {
            Logger.Log(Logger.Level.Info, "Navigated to DriveAccessDeniedPage - Initializing DriveAccessDeniedPage components");
            InitializeComponent();
            Logger.Log(Logger.Level.Debug, "DriveAccessDeniedPage components initialized");
        }

        private async void RetryButton_Click(object sender, RoutedEventArgs e)
        {
            if (ViewModel?.SelectedSync is null)
            {
                Logger.Log(Logger.Level.Warning, "");
                DetachHandlers();
                Frame.Navigate(typeof(HomePage));
            }
            else
            {
                await ViewModel.SelectedSync.Start();
            }
        }
    }
}
