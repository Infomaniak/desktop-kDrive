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

using Infomaniak.kDrive.CustomControls;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
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
            Logger.Log(Logger.Level.Debug, "SettingsPage components initialized");
        }

        private void CreateAccountButton_Click(object sender, RoutedEventArgs e)
        {
            (App.Current as App)?.StartOnboarding();
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
            ProgressRing? progressRing = FindChildProgressRing(sender as DependencyObject);
            if (progressRing is not null)
                progressRing.Visibility = Visibility.Visible;

            List<Task> loadAvailableDrivesTasks = new List<Task>();
            foreach (var user in ViewModel.Users)
            {
                loadAvailableDrivesTasks.Add(user.RefreshAvailableDrives());
            }
            await Task.WhenAll(loadAvailableDrivesTasks);

            if (progressRing is not null)
                progressRing.Visibility = Visibility.Collapsed;
        }

        private ProgressRing? FindChildProgressRing(DependencyObject? dependencyObject)
        {
            if (dependencyObject == null)
                return null;
            int childCount = VisualTreeHelper.GetChildrenCount(dependencyObject);
            for (int i = 0; i < childCount; i++)
            {
                DependencyObject child = VisualTreeHelper.GetChild(dependencyObject, i);
                if (child is ProgressRing progressRing)
                {
                    return progressRing;
                }
                ProgressRing? result = FindChildProgressRing(child);
                if (result != null)
                {
                    return result;
                }
            }
            return null;
        }

        private async void DisconectUser_Click(object sender, RoutedEventArgs e)
        {
            User? user = sender is FrameworkElement fe && fe.DataContext is User u ? u : null;
            if (user is null)
            {
                Logger.Log(Logger.Level.Error, "Unable to disconnect user: DataContext is not a User");
                return;
            }
            ContentDialog dialog = new ContentDialog();

            // XamlRoot must be set in the case of a ContentDialog running in a Desktop app
            dialog.XamlRoot = this.XamlRoot;
            dialog.Title = Utility.GetLocalizedString("Page_SettingsPage_RemoveAccount_Dialog/Title");
            dialog.PrimaryButtonText = Utility.GetLocalizedString("Page_SettingsPage_RemoveAccount_Dialog/PrimaryButtonText");
            dialog.SecondaryButtonText = Utility.GetLocalizedString("Page_SettingsPage_RemoveAccount_Dialog/SecondaryButtonText");
            dialog.DefaultButton = ContentDialogButton.Primary;
            dialog.Content = Utility.GetLocalizedString("Page_SettingsPage_RemoveAccount_Dialog/Content", user.Name);

            var result = await dialog.ShowAsync();
            if (result == ContentDialogResult.Secondary)
            {
                if (user is not null)
                {
                    var control = sender as Control;
                    if (control is not null)
                    {
                        control.IsEnabled = false;
                    }
                    await _viewModel.DisconnectUserAsync(user.DbId);
                    await Task.Delay(5000);
                    if (control is not null)
                    {
                        control.IsEnabled = true;
                    }
                }
            }
        }

        private void ManageDriveButton_Click(object sender, RoutedEventArgs e)
        {
            IDrive? drive = (sender as FrameworkElement)?.Tag as IDrive;
            if (drive is Drive)
            {
                Logger.Log(Logger.Level.Info, $"ManageDriveButton clicked for configured drive {drive.Name}, going to mange page");
                // TODO: Implement navigation to Manage Drive Page
            }
            else if (drive is DriveAvailable)
            {
                Logger.Log(Logger.Level.Info, $"ManageDriveButton clicked for unconfigured drive {drive.Name}, going to onboarding page");
                // TODO: Implement navigation to Onboarding Page
            }
            else
            {
                Logger.Log(Logger.Level.Error, "ManageDriveButton clicked but Tag is not a valid IDrive");
            }
        }

        private void SyncRulesCard_Clicked(object sender, RoutedEventArgs e)
        {
            Logger.Log(Logger.Level.Info, "Navigating to Sync Rules Page from Settings Page");
            // TODO: Implement navigation to Sync Rules Page
        }

        private async void MatomoButton_Click(object sender, RoutedEventArgs e)
        {
            if (!IsLoaded) return;
            ConsentResult result = await MatomoContentDialog.ShowAsync(this.XamlRoot);
            if (result == ConsentResult.Cancelled) return;
            await ViewModel.Settings.ChangeMatomoEnabled(result == ConsentResult.Allowed);
        }

        private async void SentryButton_Click(object sender, RoutedEventArgs e)
        {
            if (!IsLoaded) return;
            ConsentResult result = await SentryContentDialog.ShowAsync(this.XamlRoot);
            if (result == ConsentResult.Cancelled) return;
            await ViewModel.Settings.ChangeSentryEnabled(result == ConsentResult.Allowed);
        }

        private async void MatomoToogleSwitch_Toggled(object sender, RoutedEventArgs e)
        {
            if (!IsLoaded) return;
            if (sender is ToggleSwitch toggleSwitch)
            {
                toggleSwitch.IsEnabled = false;
                await ViewModel.Settings.ChangeMatomoEnabled(toggleSwitch.IsOn);
                toggleSwitch.IsEnabled = true;
            }
            else
            {
                Logger.Log(Logger.Level.Error, "MatomoToogleSwitch_Toggled: sender is not a ToggleSwitch");
            }
        }

        private async void SentryeSwitch_Toggled(object sender, RoutedEventArgs e)
        {
            if (!IsLoaded) return;
            if (sender is ToggleSwitch toggleSwitch)
            {
                toggleSwitch.IsEnabled = false;
                await ViewModel.Settings.ChangeSentryEnabled(toggleSwitch.IsOn);
                toggleSwitch.IsEnabled = true;
            }
            else
            {
                Logger.Log(Logger.Level.Error, "SentryeSwitch_Toggled: sender is not a ToggleSwitch");
            }

        }

        private async void PrivacySeeSourcesButton_Click(object sender, RoutedEventArgs e)
        {
            if (!IsLoaded) return;

            var control = sender as Control;
            if (control is not null)
                control.IsEnabled = false;

            await Windows.System.Launcher.LaunchUriAsync(App.Constants.GitHubRepoUrl);

            if (control is not null)
                control.IsEnabled = true;
        }

        private async void ProxyTypeComboBox_Changed(object sender, SelectionChangedEventArgs e)
        {
            if (!IsLoaded) return;
            var control = sender as Control;
            if (control is not null)
                control.IsEnabled = false;

            var selectedItem = e.AddedItems.OfType<ComboBoxItem>().FirstOrDefault();
            if (selectedItem is not null && Enum.TryParse<ProxyType>(selectedItem.Tag as string, out ProxyType selectedProxyType))
            {
                await ViewModel.Settings.ChangeProxyType(selectedProxyType);
            }
            else
            {
                Logger.Log(Logger.Level.Error, "ProxyTypeComboBox_Changed: selected item is null or invalid");
            }
            if (control is not null)
                control.IsEnabled = true;
        }

        private void ProxyConfig_Changed(object sender, RoutedEventArgs e)
        {
            if (!IsLoaded) return;
            if (ProxyHostTextBox.Text != ViewModel.Settings.ProxyConfig.HostName ||
               ProxyPortTextBox.Text != ViewModel.Settings.ProxyConfig.Port.ToString() ||
               ProxyNeedsAuthToggleSwitch.IsOn != ViewModel.Settings.ProxyConfig.NeedsAuth ||
               ProxyUserTextBox.Text != ViewModel.Settings.ProxyConfig.User ||
               ProxyPwdPasswordBox.Password.Length > 0)
            {
                SaveProxySettingsCard.Visibility = Visibility.Visible;
            }
            else
            {
                SaveProxySettingsCard.Visibility = Visibility.Collapsed;
            }
        }

        private async void SaveProxySettingsButton_Click(object sender, RoutedEventArgs e)
        {
            ProxySaveProgressRing.Visibility = Visibility.Visible;
            ProxySettingsExpander.IsEnabled = false;
            await ViewModel.Settings.ChangeProxyConfiguration(ProxyHostTextBox.Text, int.Parse(ProxyPortTextBox.Text), ProxyNeedsAuthToggleSwitch.IsOn, ProxyUserTextBox.Text, ProxyPwdPasswordBox.Password);
            ProxySettingsExpander.IsEnabled = true;
            SaveProxySettingsCard.Visibility = Visibility.Collapsed;
            ProxySaveProgressRing.Visibility = Visibility.Collapsed;

        }

        private void CancelProxySettingsButton_Click(object sender, RoutedEventArgs e)
        {
            ProxyHostTextBox.Text = ViewModel.Settings.ProxyConfig.HostName;
            ProxyPortTextBox.Text = ViewModel.Settings.ProxyConfig.Port.ToString();
            ProxyNeedsAuthToggleSwitch.IsOn = ViewModel.Settings.ProxyConfig.NeedsAuth;
            ProxyUserTextBox.Text = ViewModel.Settings.ProxyConfig.User;
            ProxyPwdPasswordBox.Password = string.Empty;
            SaveProxySettingsCard.Visibility = Visibility.Collapsed;
        }

        private void OpenDebugFolderButton_Click(object sender, RoutedEventArgs e)
        {
            Utility.OpenFolderSecurely(Logger.LogFolder);
        }

        private async void EnableDebugLogsToggleSwitch_Toggled(object sender, RoutedEventArgs e)
        {
            if (!IsLoaded) return;
            ToggleSwitch? toggleSwitch = sender as ToggleSwitch;

            if (toggleSwitch is null)
                Logger.Log(Logger.Level.Error, "sender is not a ToggleSwitch");

            bool toggleIsOn = toggleSwitch?.IsOn ?? false;

            if (ViewModel.Settings.LogLevel == Logger.Level.None && !toggleIsOn) return; // No change needed
            if (ViewModel.Settings.LogLevel != Logger.Level.None && toggleIsOn) return; // No change needed

            LogSettingsExpander.IsEnabled = false;
            await ViewModel.Settings.ChangeLogLevel(toggleIsOn ? Logger.Level.Debug : Logger.Level.None);
            LogSettingsExpander.IsEnabled = true;
        }

        private async void PurgeOldLogsToggleSwitch_Toggled(object sender, RoutedEventArgs e)
        {
            if (!IsLoaded) return;
            ToggleSwitch? toggleSwitch = sender as ToggleSwitch;
            if (toggleSwitch is null)
                Logger.Log(Logger.Level.Error, "sender is not a ToggleSwitch");

            if (ViewModel.Settings.PurgeOldLogs == toggleSwitch?.IsOn) return; // No change needed
            LogSettingsExpander.IsEnabled = false;
            await ViewModel.Settings.ChangePurgeOldLog(toggleSwitch?.IsOn ?? false);
            LogSettingsExpander.IsEnabled = true;
        }

        private async void ExtendedLogToggleSwitch_Toggled(object sender, RoutedEventArgs e)
        {
            if (!IsLoaded) return;
            ToggleSwitch? toggleSwitch = sender as ToggleSwitch;
            if (toggleSwitch is null)
                Logger.Log(Logger.Level.Error, "sender is not a ToggleSwitch");

            bool toggleIsOn = toggleSwitch?.IsOn ?? false;
            if (ViewModel.Settings.LogLevel == Logger.Level.Extended && toggleIsOn) return; // No change needed
            if (ViewModel.Settings.LogLevel != Logger.Level.Extended && !toggleIsOn) return; // No change needed
            LogSettingsExpander.IsEnabled = false;
            await ViewModel.Settings.ChangeLogLevel(toggleSwitch?.IsOn ?? false ? Logger.Level.Extended : Logger.Level.Debug);
            LogSettingsExpander.IsEnabled = true;
        }

        private async void LogLevelComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (!IsLoaded) return;
            var control = sender as Control;
            if (control is not null)
                control.IsEnabled = false;

            var selectedItem = e.AddedItems.OfType<ComboBoxItem>().FirstOrDefault();
            if (selectedItem is not null && Enum.TryParse<Logger.Level>(selectedItem.Tag as string, out Logger.Level selectedLevel))
            {
                await ViewModel.Settings.ChangeLogLevel(selectedLevel);
            }
            else
            {
                Logger.Log(Logger.Level.Error, "Selected item is null or invalid");
            }
            if (control is not null)
                control.IsEnabled = true;
        }

        bool _logUploadCancelled = false;
        private async void SendLogButton_Click(object sender, RoutedEventArgs e)
        {
            var result = await SendLogDialog.ShowAsync();
            if (result != ContentDialogResult.Primary) return;

            // !!! Simulate log upload progress
            // TODO: Implement actual log upload once linked with the server side

            _logUploadCancelled = false;
            SendLogsProgressRing.Value = 0;
            SendLogsProgressRing.Visibility = Visibility.Visible;
            SendLogButton.IsEnabled = false;
            CancelSendLogButton.Visibility = Visibility.Visible;
            while (SendLogsProgressRing.Value < 100 && !_logUploadCancelled)
            {
                await Task.Delay(Random.Shared.Next(200, 800));
                SendLogsProgressRing.Value += Random.Shared.Next(1, 20);
            }
            SendLogsProgressRing.Value = 100;
            await Task.Delay(500);
            SendLogsProgressRing.Visibility = Visibility.Collapsed;
            SendLogButton.IsEnabled = true;
            CancelSendLogButton.Visibility = Visibility.Collapsed;
            _logUploadCancelled = false;
        }

        private void CancelSendLogButton_Click(object sender, RoutedEventArgs e)
        {
            _logUploadCancelled = true;
        }
    }

    // templateSelector for the drives listview
    public class DriveDataTemplateSelector : DataTemplateSelector
    {
        public DataTemplate? ConfiguredTemplate { get; set; }
        public DataTemplate? UnconfiguredTemplate { get; set; }
        protected override DataTemplate? SelectTemplateCore(object item, DependencyObject container)
        {
            if (item is null) return base.SelectTemplateCore(item);
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
