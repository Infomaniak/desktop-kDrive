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
using CommunityToolkit.WinUI.Controls;
using Infomaniak.kDrive.Analytics;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System;
using System.ComponentModel;
using System.Linq;
namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class UpdateExpander : SettingsExpander
    {
        private readonly IAnalyticsService _analyticsService = App.ServiceProvider.GetRequiredService<IAnalyticsService>();

        private readonly AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        public AppModel ViewModel => _viewModel;

        public UpdateExpander()
        {
            InitializeComponent();

        }
        private void UserControl_Unloaded(object sender, RoutedEventArgs e)
        {
            UnregisterPropertyChangedHandlers();
        }
        private void UpdateExpander_Loaded(object sender, RoutedEventArgs e)
        {
            RegisterPropertyChangedHandlers();
            UpdateInternalChannelComboBoxVisibility();
        }

        private bool IsStaffUserConnected()
        {
            return ViewModel.Users.Any(u => u.IsStaff && u.IsConnected);
        }

        private void UpdateInternalChannelComboBoxVisibility()
        {
            Visibility visibility = IsStaffUserConnected() ? Visibility.Visible : Visibility.Collapsed;
            UpdateChannelComboBox_Internal.Visibility = visibility;
            UpdateChannelComboBox_Test.Visibility = visibility;
        }

        private void RegisterPropertyChangedHandlers()
        {
            ViewModel.Settings.UpdateManager.PropertyChanged += OnSettingsPropertyChanged;
            ViewModel.Settings.PropertyChanged += OnSettingsPropertyChanged;
            OnSettingsPropertyChanged(null, null);
        }

        private void UnregisterPropertyChangedHandlers()
        {
            ViewModel.Settings.UpdateManager.PropertyChanged -= OnSettingsPropertyChanged;
            ViewModel.Settings.PropertyChanged -= OnSettingsPropertyChanged;
        }

        private void OnSettingsPropertyChanged(object? sender, PropertyChangedEventArgs? e)
        {
            if (e is null || e.PropertyName == nameof(UpdateManager.AvailableUpdate) || e.PropertyName == nameof(Settings.AppVersion))
                Refresh();
        }

        public void Refresh()
        {
            if (ViewModel.Settings is null)
            {
                Logger.Log(Logger.Level.Warning, "Settings is null, this is unexpected.");
                return;
            }

            if (ViewModel.Settings.UpdateManager.AvailableUpdate is AppVersion updateVersion)
            {
                base.Description = Localizer.Instance.GetString("updateAvailable", $"{updateVersion.Tag}.{updateVersion.BuildVersion}");
            }
            else if (ViewModel.Settings.AppVersion is AppVersion AppVersionInfo)
            {
                base.Description = Localizer.Instance.GetString("appUpToDate");
            }
            UpdateInternalChannelComboBoxVisibility();
        }

        private async void UpdateChannel_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (!IsLoaded)
                return;

            if (sender is ComboBox comboBox && comboBox.SelectedItem is ComboBoxItem selectedItem)
            {
                comboBox.IsEnabled = false;
                string? channelString = selectedItem.Tag as string;
                if (Enum.TryParse<DistributionChannel>(channelString, out DistributionChannel selectedChannel))
                {
                    if (selectedChannel == ViewModel.Settings.UpdateManager.CurrentChannel)
                    {
                        Logger.Log(Logger.Level.Info, $"Selected update channel {selectedChannel} is the same as the current channel, no change needed.");
                        comboBox.IsEnabled = true;
                        return;
                    }

                    _analyticsService.TrackClick(Analytics.Keys.Category.GeneralSettingsPage, Analytics.Keys.EventName.ChangeReleaseChannel);

                    if (!await ViewModel.Settings.UpdateManager.ChangeChannel(selectedChannel))
                    {
                        Logger.Log(Logger.Level.Error, $"Failed to change update channel to {selectedChannel}");
                        Utility.ShowUnexpectedErrorTeachingTip();
                        comboBox.IsEnabled = true;
                        return;
                    }
                    Logger.Log(Logger.Level.Info, $"Update channel changed to {selectedChannel}");
                }
                else
                {
                    Logger.Log(Logger.Level.Error, $"Invalid update channel selected: {channelString}");
                }
                comboBox.IsEnabled = true;
            }
        }

        private async void UpdateButton_Click(object sender, RoutedEventArgs e)
        {
            if (ViewModel.Settings is null)
            {
                Logger.Log(Logger.Level.Error, "Settings is null, cannot start update process.");
                return;
            }

            var btn = sender as Button;
            if (btn is not null)
            {
                btn.IsEnabled = false;
                Logger.Log(Logger.Level.Info, "User clicked on Update button, starting update process.");
                _analyticsService.TrackClick(Analytics.Keys.Category.GeneralSettingsPage, Analytics.Keys.EventName.StartUpdate);
            }

            if (!await UpdateManager.StartUpdate())
            {
                Logger.Log(Logger.Level.Error, "Update process failed to start.");
                Utility.ShowUnexpectedErrorTeachingTip();
            }

            if (btn is not null)
            {
                btn.IsEnabled = true;
            }
        }

        private async void AutoUpdateToggleSwitch_Tapped(object sender, Microsoft.UI.Xaml.Input.TappedRoutedEventArgs e)
        {
            if (!IsLoaded)
                return;
            if (sender is ToggleSwitch toggleSwitch)
            {
                toggleSwitch.IsEnabled = false;
                if (ViewModel.Settings is null || ViewModel.Settings.UpdateManager is null)
                {
                    Logger.Log(Logger.Level.Error, "Settings or UpdateManager is null, cannot change auto-update setting.");
                    toggleSwitch.IsEnabled = true;
                    return;
                }

                if (toggleSwitch.IsOn == ViewModel.Settings.UpdateManager.AutoUpdateEnabled)
                {
                    Logger.Log(Logger.Level.Info, "Auto-update toggle switch state is the same as the current setting, no change needed.");
                    toggleSwitch.IsEnabled = true;
                    return;
                }

                _analyticsService.TrackClick(Analytics.Keys.Category.GeneralSettingsPage, Analytics.Keys.EventName.ChangeAutoUpdate, toggleSwitch.IsOn ? 1 : 0);

                if (!await ViewModel.Settings.UpdateManager.ChangeAutoUpdate(toggleSwitch.IsOn))
                {
                    Logger.Log(Logger.Level.Error, "Failed to change auto-update setting.");
                    Utility.ShowUnexpectedErrorTeachingTip();
                }
                toggleSwitch.IsEnabled = true;
            }
        }

        private void KnowMoreButton_Click(object sender, RoutedEventArgs e)
        {
            (App.Current as App)?.ShowUpdateWindow();
        }
    }
}
