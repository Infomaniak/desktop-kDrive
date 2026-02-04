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
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.Pages.Settings
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

        protected override async void OnNavigatedTo(NavigationEventArgs e)
        {
            await RefreshAvailableDrivesForAllUsers();
        }

        private void CreateAccountButton_Click(object sender, RoutedEventArgs e)
        {
            (App.Current as App)?.StartOnboarding();
        }

        private async void AutoStartToggleSwitch_Tapped(object sender, TappedRoutedEventArgs e)
        {
            if (!IsLoaded)
                return;
            if (sender is ToggleSwitch toggleSwitch)
            {
                toggleSwitch.IsEnabled = false;
                if (!await ViewModel.Settings.ChangeAutoStart(toggleSwitch.IsOn))
                {
                    Logger.Log(Logger.Level.Error, "Failed to change AutoStart setting");
                    Utility.ShowUnexpectedErrorTeachingTip();
                }
                toggleSwitch.IsEnabled = true;
            }
        }

        private async void NotificationsCombobox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (!IsLoaded)
                return;
            if (sender is ComboBox comboBox && comboBox.SelectedItem is ComboBoxItem selectedItem)
            {
                string? selection = selectedItem.Tag as string;
                if (Enum.TryParse<NotificationsDisabled>(selection, out NotificationsDisabled selectedNotificationsDisabled))
                {
                    if (!await ViewModel.Settings.ChangeNotificationsDisabled(selectedNotificationsDisabled))
                    {
                        Logger.Log(Logger.Level.Error, $"Failed to change update notifications disabled to {selectedNotificationsDisabled}");
                        Utility.ShowUnexpectedErrorTeachingTip();
                        return;
                    }
                    Logger.Log(Logger.Level.Info, $"Update notifications disabled changed to {selectedNotificationsDisabled}");
                }
                else
                {
                    Logger.Log(Logger.Level.Error, $"Update notifications disabled changed to  {selectedNotificationsDisabled}");
                }
            }
        }

        private async void MoveToTrashToggleSwitch_Tapped(object sender, TappedRoutedEventArgs e)
        {
            if (!IsLoaded)
                return;
            if (sender is ToggleSwitch toggleSwitch)
            {
                toggleSwitch.IsEnabled = false;
                if (!await ViewModel.Settings.ChangeMoveToTrash(toggleSwitch.IsOn))
                {
                    Logger.Log(Logger.Level.Error, "Failed to change MoveToTrash setting");
                    Utility.ShowUnexpectedErrorTeachingTip();
                }
                toggleSwitch.IsEnabled = true;
            }
        }

        private async void UserSettingsExpander_Expanded(object sender, EventArgs e)
        {
            User? user = (sender as FrameworkElement)?.DataContext as User;
            if (user is null)
            {
                Logger.Log(Logger.Level.Error, "Unable to find the user from DataContext.");
                return;
            }

            if (!await user.RefreshAvailableDrives(CancellationToken.None))
            {
                Logger.Log(Logger.Level.Warning, "Error while refreshing available drives for user.");
                Utility.ShowUnexpectedErrorTeachingTip(); // Show a generic error message for now, discussion is in progress with UX team to improve this.
            }
        }

        private async Task RefreshAvailableDrivesForAllUsers()
        {
            List<Task<bool>> loadAvailableDrivesTasks = new List<Task<bool>>();
            foreach (var user in ViewModel.Users)
            {
                loadAvailableDrivesTasks.Add(user.RefreshAvailableDrives(CancellationToken.None));
            }
            await Task.WhenAll(loadAvailableDrivesTasks);
            // Results are ignored for now; errors are displayed only if the user explicitly expands the user settings.
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
            dialog.XamlRoot = this.XamlRoot;
            dialog.Title = Utility.GetLocalizedString("Page_SettingsPage_RemoveAccount_Dialog/Title");
            dialog.PrimaryButtonText = Utility.GetLocalizedString("Page_SettingsPage_RemoveAccount_Dialog/PrimaryButtonText");
            dialog.SecondaryButtonText = Utility.GetLocalizedString("Page_SettingsPage_RemoveAccount_Dialog/SecondaryButtonText");
            dialog.DefaultButton = ContentDialogButton.Primary;
            dialog.Content = Utility.GetLocalizedString("Page_SettingsPage_RemoveAccount_Dialog/Content", user.Name);

            var result = await dialog.ShowAsync();
            if (result == ContentDialogResult.Secondary)
            {
                var control = sender as Control;
                if (control is not null)
                    control.IsEnabled = false;

                await _viewModel.DisconnectUserAsync(user.DbId);

                if (control is not null)
                    control.IsEnabled = true;
            }
        }

        private void ManageDriveButton_Click(object sender, RoutedEventArgs e)
        {
            IDrive? drive = (sender as FrameworkElement)?.Tag as IDrive;
            if (drive is not null)
            {
                Logger.Log(Logger.Level.Info, $"ManageDriveButton clicked for configured drive {drive.Name}, going to manage page");
                Frame.Navigate(typeof(DriveManagementPage), drive);
            }
            else
            {
                Logger.Log(Logger.Level.Error, "ManageDriveButton clicked but Tag is not a valid IDrive");
            }
        }

        private void SyncRulesCard_Clicked(object sender, RoutedEventArgs e)
        {
            Logger.Log(Logger.Level.Info, "Navigating to Sync Rules Page from Settings Page");
            Frame.Navigate(typeof(TemplateExclusionPage));
        }

        private async void MatomoButton_Click(object sender, RoutedEventArgs e)
        {
            if (!IsLoaded)
                return;
            ConsentResult result = await MatomoContentDialog.ShowAsync(this.XamlRoot);
            if (result == ConsentResult.Cancelled)
                return;
            if (!await ViewModel.Settings.ChangeMatomoEnabled(result == ConsentResult.Allowed))
            {
                Logger.Log(Logger.Level.Error, "Failed to change Matomo enabled setting");
                Utility.ShowUnexpectedErrorTeachingTip();
            }
        }

        private async void SentryButton_Click(object sender, RoutedEventArgs e)
        {
            if (!IsLoaded)
                return;
            ConsentResult result = await SentryContentDialog.ShowAsync(this.XamlRoot);
            if (result == ConsentResult.Cancelled)
                return;

            if (!await ViewModel.Settings.ChangeSentryEnabled(result == ConsentResult.Allowed))
            {
                Logger.Log(Logger.Level.Error, "Failed to change Sentry enabled setting");
                Utility.ShowUnexpectedErrorTeachingTip();
            }
        }

        private async void PrivacySeeSourcesButton_Click(object sender, RoutedEventArgs e)
        {
            if (!IsLoaded)
                return;

            var control = sender as Control;
            if (control is not null)
                control.IsEnabled = false;

            await Windows.System.Launcher.LaunchUriAsync(App.Constants.GitHub.RepoUrl);

            if (control is not null)
                control.IsEnabled = true;
        }

        private async void ProxyTypeComboBox_Changed(object sender, SelectionChangedEventArgs e)
        {
            if (!IsLoaded)
                return;
            var control = sender as Control;
            if (control is not null)
                control.IsEnabled = false;

            var selectedItem = e.AddedItems.OfType<ComboBoxItem>().FirstOrDefault();
            if (selectedItem is not null && Enum.TryParse<ProxyType>(selectedItem.Tag as string, out ProxyType selectedProxyType))
            {
                if (!await ViewModel.Settings.ChangeProxyType(selectedProxyType))
                {
                    Logger.Log(Logger.Level.Error, "Failed to change proxy type");
                    Utility.ShowUnexpectedErrorTeachingTip();
                }
            }
            else
            {
                Logger.Log(Logger.Level.Error, "selected item is null or invalid");
            }
            if (control is not null)
                control.IsEnabled = true;
        }

        private void ProxyConfig_Changed(object sender, RoutedEventArgs e)
        {
            if (!IsLoaded)
                return;
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
            if (!await ViewModel.Settings.ChangeProxyConfiguration(ProxyHostTextBox.Text, int.Parse(ProxyPortTextBox.Text), ProxyNeedsAuthToggleSwitch.IsOn, ProxyUserTextBox.Text, ProxyPwdPasswordBox.Password))
            {
                Logger.Log(Logger.Level.Error, "Failed to change proxy configuration");
                Utility.ShowUnexpectedErrorTeachingTip();
            }
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

        private async void EnableDebugLogsToggleSwitch_Tapped(object sender, TappedRoutedEventArgs e)
        {
            if (!IsLoaded)
                return;
            ToggleSwitch? toggleSwitch = sender as ToggleSwitch;

            if (toggleSwitch is null)
                Logger.Log(Logger.Level.Error, "sender is not a ToggleSwitch");

            bool toggleIsOn = toggleSwitch?.IsOn ?? false;

            if (ViewModel.Settings.LogLevel == Logger.Level.None && !toggleIsOn)
                return; // No change needed
            if (ViewModel.Settings.LogLevel != Logger.Level.None && toggleIsOn)
                return; // No change needed

            LogSettingsExpander.IsEnabled = false;
            if (!await ViewModel.Settings.ChangeLogLevel(toggleIsOn ? Logger.Level.Debug : Logger.Level.None))
            {
                Logger.Log(Logger.Level.Error, "Failed to change log level");
                Utility.ShowUnexpectedErrorTeachingTip();
            }
            LogSettingsExpander.IsEnabled = true;
        }

        private async void PurgeOldLogsToggleSwitch_Tapped(object sender, TappedRoutedEventArgs e)
        {
            if (!IsLoaded)
                return;
            ToggleSwitch? toggleSwitch = sender as ToggleSwitch;
            if (toggleSwitch is null)
            {
                Logger.Log(Logger.Level.Error, "sender is not a ToggleSwitch");
                return;
            }

            if (ViewModel.Settings.PurgeOldLogs == toggleSwitch.IsOn)
                return; // No change needed
            LogSettingsExpander.IsEnabled = false;
            if (!await ViewModel.Settings.ChangePurgeOldLog(toggleSwitch.IsOn))
            {
                Logger.Log(Logger.Level.Error, "Failed to change purge old logs setting");
                Utility.ShowUnexpectedErrorTeachingTip();
            }
            LogSettingsExpander.IsEnabled = true;
        }

        private async void ExtendedLogToggleSwitch_Tapped(object sender, TappedRoutedEventArgs e)
        {
            if (!IsLoaded)
                return;
            ToggleSwitch? toggleSwitch = sender as ToggleSwitch;
            if (toggleSwitch is null)
                Logger.Log(Logger.Level.Error, "sender is not a ToggleSwitch");

            bool toggleIsOn = toggleSwitch?.IsOn ?? false;
            if (ViewModel.Settings.LogLevel == Logger.Level.Extended && toggleIsOn)
                return; // No change needed
            if (ViewModel.Settings.LogLevel != Logger.Level.Extended && !toggleIsOn)
                return; // No change needed
            LogSettingsExpander.IsEnabled = false;
            if (!await ViewModel.Settings.ChangeLogLevel(toggleSwitch?.IsOn ?? false ? Logger.Level.Extended : Logger.Level.Debug))
            {
                Logger.Log(Logger.Level.Error, "Failed to change log level");
                Utility.ShowUnexpectedErrorTeachingTip();
            }
            LogSettingsExpander.IsEnabled = true;
        }

        private async void LogLevelComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (!IsLoaded)
                return;
            var control = sender as Control;
            if (control is not null)
                control.IsEnabled = false;

            var selectedItem = e.AddedItems.OfType<ComboBoxItem>().FirstOrDefault();
            if (selectedItem is not null && Enum.TryParse<Logger.Level>(selectedItem.Tag as string, out Logger.Level selectedLevel))
            {
                if (!await ViewModel.Settings.ChangeLogLevel(selectedLevel))
                {
                    Logger.Log(Logger.Level.Error, "LogLevelComboBox_SelectionChanged: Failed to change log level");
                    Utility.ShowUnexpectedErrorTeachingTip();
                }
            }
            else
            {
                Logger.Log(Logger.Level.Error, "Selected item is null or invalid");
            }
            if (control is not null)
                control.IsEnabled = true;
        }
    }

    // templateSelector for the drives listview
    public partial class DriveDataTemplateSelector : DataTemplateSelector
    {
        public DataTemplate? ConfiguredTemplate { get; set; }
        public DataTemplate? UnconfiguredTemplate { get; set; }
        protected override DataTemplate? SelectTemplateCore(object item, DependencyObject container)
        {
            if (item is null)
                return base.SelectTemplateCore(item);
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
