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
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System;
using System.ComponentModel;
using System.Linq;

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
            RegisterPropertyChangedHandlers();
            Loaded += onPageLoaded;
            

            Logger.Log(Logger.Level.Debug, "SettingsPage components initialized");
        }
        ~SettingsPage()
        {
            UnregisterPropertyChangedHandlers();
        }

        void onPageLoaded(object sender, RoutedEventArgs e)
        {
            // check if any of the users is staff to show the internal update channel combobox
            bool staffUserExists = ViewModel.Users.Any(user => user.IsStaff && user.IsConnected);
            UpdateChannelComboBox_Internal.Visibility = staffUserExists ? Visibility.Visible : Visibility.Collapsed;
        }

        private void RegisterPropertyChangedHandlers()
        {
            ViewModel.Settings.PropertyChanged += Settings_PropertyChanged;
            SetEnumComboBoxSelection(NotificationsComboBox, ViewModel.Settings.NotificationsDisabled);

            ViewModel.Settings.UpdateManager.PropertyChanged += UpdateManager_PropertyChanged;
            SetEnumComboBoxSelection(UpdateChannelComboBox, ViewModel.Settings.UpdateManager.CurrentChannel);
        }

        private void Settings_PropertyChanged(object? sender, PropertyChangedEventArgs e)
        {
            if (e.PropertyName == nameof(ViewModel.Settings.NotificationsDisabled))
                SetEnumComboBoxSelection(NotificationsComboBox, ViewModel.Settings.NotificationsDisabled);
        }

        private void UpdateManager_PropertyChanged(object? sender, PropertyChangedEventArgs e)
        {
            if (e.PropertyName == nameof(ViewModel.Settings.UpdateManager.CurrentChannel))
                SetEnumComboBoxSelection(UpdateChannelComboBox, ViewModel.Settings.UpdateManager.CurrentChannel);

        }
        private void UnregisterPropertyChangedHandlers()
        {
            ViewModel.Settings.UpdateManager.PropertyChanged -= UpdateManager_PropertyChanged;
        }

        private void CreateAccountButton_Click(object sender, RoutedEventArgs e)
        {
            (App.Current as App)?.StartOnboarding();
        }

        private async void UpdateChannel_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (!IsLoaded) return;
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
                    Logger.Log(Logger.Level.Error, $"Invalid update channel selected: {channelString}");
                }
            }
        }

        private async void AutoUpdateToggleSwitch_Toggled(object sender, RoutedEventArgs e)
        {
            if (!IsLoaded) return;
            if (sender is ToggleSwitch toggleSwitch)
            {
                toggleSwitch.IsEnabled = false;
                await ViewModel.Settings.UpdateManager.ChangeAutoUpdate(toggleSwitch.IsOn);
                toggleSwitch.IsEnabled = true;
            }
        }

        private async void AutoStartToggleSwitch_Toggled(object sender, RoutedEventArgs e)
        {
            if (!IsLoaded) return;
            if (sender is ToggleSwitch toggleSwitch)
            {
                toggleSwitch.IsEnabled = false;
                await ViewModel.Settings.ChangeAutoStart(toggleSwitch.IsOn);
                toggleSwitch.IsEnabled = true;
            }
        }

        private void SetEnumComboBoxSelection<TEnum>(ComboBox comboBox, TEnum value) where TEnum : struct, Enum
        {
            foreach (var item in comboBox.Items)
            {
                if (item is ComboBoxItem comboBoxItem && comboBoxItem.Tag is string tagString && Enum.TryParse<TEnum>(tagString, out TEnum itemValue) && itemValue.Equals(value))
                {
                    comboBox.SelectedItem = comboBoxItem;
                    return;
                }
            }
        }
        private async void NotificationsCombobox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (!IsLoaded) return;
            if (sender is ComboBox comboBox && comboBox.SelectedItem is ComboBoxItem selectedItem)
            {
                string? selection = selectedItem.Tag as string;
                if (Enum.TryParse<NotificationsDisabled>(selection, out NotificationsDisabled selectedNotificationsDisabled))
                {
                    await ViewModel.Settings.ChangeNotificationsDisabled(selectedNotificationsDisabled);
                    Logger.Log(Logger.Level.Info, $"Update notifications disabled changed to {selectedNotificationsDisabled}");
                }
                else
                {
                    Logger.Log(Logger.Level.Error, $"Update notifications disabled changed to  {selectedNotificationsDisabled}");
                }
            }
        }

        private async void MoveToTrashToggleSwitch_Toggled(object sender, RoutedEventArgs e)
        {
            if (!IsLoaded) return;
            if (sender is ToggleSwitch toggleSwitch)
            {
                toggleSwitch.IsEnabled = false;
                await ViewModel.Settings.ChangeMoveToTrash(toggleSwitch.IsOn);
                toggleSwitch.IsEnabled = true;
            }
        }
    }
}
