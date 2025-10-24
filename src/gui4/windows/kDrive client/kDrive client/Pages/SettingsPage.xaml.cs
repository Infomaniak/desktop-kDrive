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
using System;
using System.ComponentModel;
using System.Globalization;
using System.Threading;

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
            SetSelectedChannel(ViewModel.Settings.UpdateManager.CurrentChannel);
            RegisterPropertyChangedHandlers();
            Logger.Log(Logger.Level.Debug, "SettingsPage components initialized");
        }
        ~SettingsPage()
        {
            UnregisterPropertyChangedHandlers();
        }

        private void RegisterPropertyChangedHandlers()
        {
            ViewModel.Settings.UpdateManager.PropertyChanged += UpdateManager_PropertyChanged;
        }

        private void UpdateManager_PropertyChanged(object? sender, PropertyChangedEventArgs e)
        {
            if (e.PropertyName == nameof(ViewModel.Settings.UpdateManager.CurrentChannel))
            {
                SetSelectedChannel(ViewModel.Settings.UpdateManager.CurrentChannel);
            }
        }

        private void UnregisterPropertyChangedHandlers()
        {
            ViewModel.Settings.UpdateManager.PropertyChanged -= UpdateManager_PropertyChanged;
        }


        private void CreateAccountButton_Click(object sender, RoutedEventArgs e)
        {
            (App.Current as App)?.StartOnboarding();
        }

        private CancellationTokenSource? _updateChannelcancellationTokenSource = null;
        private async void UpdateChannel_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (sender is ComboBox comboBox && comboBox.SelectedItem is ComboBoxItem selectedItem)
            {
                string? channelString = selectedItem.Tag as string;
                if (Enum.TryParse<VersionChannel>(channelString, out VersionChannel selectedChannel))
                {
                    await ViewModel.Settings.UpdateManager.ChangeChannel(selectedChannel);
                    Logger.Log(Logger.Level.Info, $"Update channel changed to {selectedChannel}");
                }
                else
                {
                    Logger.Log(Logger.Level.Warning, $"Invalid update channel selected: {channelString}");
                }
            }
        }

        private void SetSelectedChannel(VersionChannel channel)
        {
            foreach (var item in UpdateChannel.Items)
            {
                if (item is ComboBoxItem comboBoxItem && comboBoxItem.Tag is string tagString && Enum.TryParse<VersionChannel>(tagString, out VersionChannel itemChannel) && itemChannel == channel)
                {
                    UpdateChannel.SelectedItem = comboBoxItem;
                    return;
                }
            }
        }
    }
}
