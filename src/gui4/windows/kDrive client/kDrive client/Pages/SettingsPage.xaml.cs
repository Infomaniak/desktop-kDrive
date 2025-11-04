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
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Threading.Tasks;

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

        private async void UserSettingsExpander_Expanded(object sender, EventArgs e)
        {
            List<Task> loadAvailableDrivesTasks = new List<Task>();
            foreach (var user in ViewModel.Users)
            {
                loadAvailableDrivesTasks.Add(user.RefreshAvailableDrives());
            }
            await Task.WhenAll(loadAvailableDrivesTasks);
        }

        private void SettingsCard_Click(object sender, RoutedEventArgs e)
        {

        }

        private async void DisconectUser_Click(object sender, RoutedEventArgs e)
        {

            User? user = sender is FrameworkElement fe && fe.DataContext is User u ? u : null;
            if(user is null)
            {
                Logger.Log(Logger.Level.Error, "Unable to disconnect user: DataContext is not a User");

            }
            ContentDialog dialog = new ContentDialog();

            // XamlRoot must be set in the case of a ContentDialog running in a Desktop app
            dialog.XamlRoot = this.XamlRoot;
            dialog.Title = "Déconnecter ce compte ?";
            dialog.PrimaryButtonText = "Garder le compte";
            dialog.SecondaryButtonText = "Deconnecter";
            dialog.DefaultButton = ContentDialogButton.Primary;

            dialog.Content = $"Vous allez déconnecter {user?.Name ?? "l'utilisateur"} de l’application.\r\nLes fichiers synchronisés depuis l’ensemble des kDrives de ce compte seront retirés de votre ordinateur.\r\n\r\nToutes vos données resteront accessibles en ligne sur kDrive.";

            
            var result = await dialog.ShowAsync();
            if(result == ContentDialogResult.Secondary)
            {
                if(user is not null)
                {
                    await _viewModel.DisconnectUserAsync(user.DbId);
                }
            }
        }

        private void ManageDriveButton_Click(object sender, RoutedEventArgs e)
        {
            Logger.Log(Logger.Level.Info, $"ManageDriveButton clicked for drive {((sender as FrameworkElement)?.Tag as IDrive)?.Name}");
        }

        private void ConfigureDriveButton_Click(object sender, RoutedEventArgs e)
        {
            Logger.Log(Logger.Level.Info, $"ConfigureDrive clicked for available drive {((sender as FrameworkElement)?.Tag as IDrive)?.Name}");

        }
    }

    // templateSelector for the drives listview
    public class DriveDataTemplateSelector : DataTemplateSelector
    {
        public DataTemplate? ConfiguredTemplate { get; set; }
        public DataTemplate? UnconfiguredTemplate { get; set; }
        protected override DataTemplate? SelectTemplateCore(object item, DependencyObject container)
        {
            if (item is DriveAvailable)
                return UnconfiguredTemplate;
            else if (item is Drive)
                return ConfiguredTemplate;
            else
                Logger.Log(Logger.Level.Error, "Unknown item type in SelectTemplateCore");

            return base.SelectTemplateCore(item);
        }


    }
}
