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
using CommunityToolkit.WinUI.Controls;
using Infomaniak.kDrive.CustomControls;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.Pages.Settings
{
    public sealed partial class SettingsPage : Page
    {
        private readonly IAnalyticsService _analyticsService = App.ServiceProvider.GetRequiredService<IAnalyticsService>();
        private readonly AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        private const string _skipNextRefreshKey = "skipNextRefresh";
        private NavigationParameter? _navigationParameter;
        static private double _lastVerticalOffset = 0;

        public AppModel ViewModel => _viewModel;

        public struct NavigationParameter
        {
            public enum SettingsTab
            {
                Default,
                Users,
                LastVerticalOffset
            }

            public SettingsTab Tab { get; set; }
            public User? UserToShow { get; set; }
        }

        public SettingsPage()
        {
            Logger.Log(Logger.Level.Info, "Navigated to SettingsPage - Initializing SettingsPage components");
            InitializeComponent();
            Logger.Log(Logger.Level.Debug, "SettingsPage components initialized");
            Loaded += SettingsPage_Loaded;
        }

        private void SettingsPage_Loaded(object sender, RoutedEventArgs e)
        {
            if (_navigationParameter is null)
                return;

            switch (_navigationParameter.Value.Tab)
            {
                case NavigationParameter.SettingsTab.Users:
                    BringUserIntoView(_navigationParameter.Value.UserToShow);
                    break;
                case NavigationParameter.SettingsTab.LastVerticalOffset:
                    SettingsScrollView.ScrollTo(0, _lastVerticalOffset, new ScrollingScrollOptions(ScrollingAnimationMode.Disabled, ScrollingSnapPointsMode.Ignore));
                    break;
                default:
                    break;
            }
        }

        protected override void OnNavigatedTo(NavigationEventArgs e)
        {
            _analyticsService.TrackPageView(Analytics.Keys.Category.SettingsPage);
            _navigationParameter = e.Parameter as NavigationParameter?;

            if (e.NavigationMode != NavigationMode.New)
            {
                _navigationParameter = new NavigationParameter { Tab = NavigationParameter.SettingsTab.LastVerticalOffset };
            }
        }

        protected override void OnNavigatedFrom(NavigationEventArgs e)
        {
            _lastVerticalOffset = SettingsScrollView.VerticalOffset;
        }

        private void BringUserIntoView(User? user)
        {
            if (user is not null && UsersListView.ContainerFromItem(user) is UIElement container)
            {
                container.StartBringIntoView(new BringIntoViewOptions { VerticalAlignmentRatio = 0.1f });
                return;
            }

            // If the container is null, just scroll to the Accounts section
            AccountsStackPanel.StartBringIntoView(new BringIntoViewOptions { VerticalAlignmentRatio = 0.1f });
        }

        private void CreateAccountButton_Click(object sender, RoutedEventArgs e)
        {
            _analyticsService.TrackClick(Analytics.Keys.Category.AccountsSettingsPage, Analytics.Keys.EventName.OpenOnboarding);
            (App.Current as App)?.StartOnboarding();
        }

        private async void AutoStartToggleSwitch_Toggled(object sender, RoutedEventArgs e)
        {
            if (sender is ToggleSwitch toggleSwitch)
            {
                if (!toggleSwitch.IsEnabled || !toggleSwitch.IsLoaded)
                    return;

                toggleSwitch.IsEnabled = false;
                _analyticsService.TrackClick(Analytics.Keys.Category.GeneralSettingsPage, Analytics.Keys.EventName.ChangeAutoStart, toggleSwitch.IsOn ? 1 : 0);
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
                if (!comboBox.IsEnabled)
                    return;

                comboBox.IsEnabled = false;
                _analyticsService.TrackClick(Analytics.Keys.Category.GeneralSettingsPage, Analytics.Keys.EventName.ChangeNotifications);

                string? selection = selectedItem.Tag as string;
                if (!Enum.TryParse<NotificationsDisabled>(selection, out NotificationsDisabled selectedNotificationsDisabled))
                {
                    Logger.Log(Logger.Level.Error, $"Invalid selection for NotificationsDisabled: {selection}");
                    comboBox.IsEnabled = true;
                    return;
                }

                if (!await ViewModel.Settings.ChangeNotificationsDisabled(selectedNotificationsDisabled))
                {
                    Logger.Log(Logger.Level.Error, $"Failed to change update notifications disabled to {selectedNotificationsDisabled}");
                    Utility.ShowUnexpectedErrorTeachingTip();
                    comboBox.IsEnabled = true;
                    return;
                }

                Logger.Log(Logger.Level.Info, $"Update notifications disabled changed to {selectedNotificationsDisabled}");
                comboBox.IsEnabled = true;
            }
        }

        private async void MoveToTrashToggleSwitch_Toggled(object sender, RoutedEventArgs e)
        {
            if (sender is ToggleSwitch toggleSwitch)
            {
                if (!toggleSwitch.IsEnabled || !toggleSwitch.IsLoaded)
                    return;
                _analyticsService.TrackClick(Analytics.Keys.Category.GeneralSettingsPage, Analytics.Keys.EventName.ChangeMoveToTrash);

                toggleSwitch.IsEnabled = false;
                if (!await ViewModel.Settings.ChangeMoveToTrash(toggleSwitch.IsOn))
                {
                    Logger.Log(Logger.Level.Error, "Failed to change MoveToTrash setting");
                    Utility.ShowUnexpectedErrorTeachingTip();
                }
                toggleSwitch.IsEnabled = true;
            }
        }

        private async void UserSettingsExpander_Loaded(object sender, RoutedEventArgs e)
        {
            User? user = (sender as FrameworkElement)?.DataContext as User;
            if (user is null)
            {
                Logger.Log(Logger.Level.Error, "Unable to find the user from DataContext.");
                return;
            }

            if (await user.RefreshAvailableDrives(CancellationToken.None))
            {
                var senderExpander = sender as SettingsExpander;
                if (senderExpander is null)
                {
                    Logger.Log(Logger.Level.Error, "Unable to find the SettingsExpander from sender.");
                    return;
                }
            }
            else
            {
                Logger.Log(Logger.Level.Warning, "Error while refreshing available drives for user.");
            }

        }

        private async void UserSettingsExpander_Expanded(object sender, EventArgs e)
        {
            if (!IsLoaded)
                return;

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

        private void FixForegroundOnPointerExited(object sender, Microsoft.UI.Xaml.Input.PointerRoutedEventArgs e)
        {
            if (sender is Control control)
            {
                var currentForeground = control.Foreground;
                control.Foreground = null;
                control.Foreground = currentForeground;
            }
        }

        private async void DisconectUser_Click(object sender, RoutedEventArgs e)
        {
            User? user = sender is FrameworkElement fe && fe.DataContext is User u ? u : null;
            if (user is null)
            {
                Logger.Log(Logger.Level.Error, "Unable to disconnect user: DataContext is not a User");
                return;
            }
            var control = sender as Control;
            if (control is not null)
                control.IsEnabled = false;
            _analyticsService.TrackClick(Analytics.Keys.Category.AccountsSettingsPage, Analytics.Keys.EventName.Disconnect);

            ContentDialog dialog = new ContentDialog
            {
                XamlRoot = this.XamlRoot,
                Title = Localizer.Instance.GetString("dialogRemoveAccountTitle"),
                PrimaryButtonText = Localizer.Instance.GetString("buttonKeepAccount"),
                SecondaryButtonText = Localizer.Instance.GetString("buttonLogOut"),
                DefaultButton = ContentDialogButton.Primary,
                Content = Localizer.Instance.GetString("dialogRemoveAccountContent", user.Name)
            };

            var result = await dialog.ShowAsync();
            if (result == ContentDialogResult.Secondary)
            {
                _analyticsService.TrackClick(Analytics.Keys.Category.AccountsSettingsPage, Analytics.Keys.EventName.ConfirmDisconnect);
                if (!await _viewModel.DisconnectUserAsync(user.DbId))
                {
                    Logger.Log(Logger.Level.Error, "Failed to disconnect user");
                    Utility.ShowUnexpectedErrorTeachingTip();
                    if (control is not null)
                        control.IsEnabled = true;
                    return;
                }
            }
            else
            {
                _analyticsService.TrackClick(Analytics.Keys.Category.AccountsSettingsPage, Analytics.Keys.EventName.CancelDisconnect);
                Logger.Log(Logger.Level.Info, "User disconnection cancelled by user");
            }
            if (control is not null)
                control.IsEnabled = true;
        }

        private void ManageDriveButton_Click(object sender, RoutedEventArgs e)
        {
            IDrive? drive = (sender as FrameworkElement)?.Tag as IDrive;
            if (drive is not null)
            {
                Logger.Log(Logger.Level.Info, $"ManageDriveButton clicked for configured drive {drive.Name}, going to manage page");
                _analyticsService.TrackClick(Analytics.Keys.Category.AccountsSettingsPage, Analytics.Keys.EventName.OpenDriveSettings);
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
            _analyticsService.TrackClick(Analytics.Keys.Category.AdvancedSettingsPage, Analytics.Keys.EventName.OpenExclusionRules);
            Frame.Navigate(typeof(TemplateExclusionPage));
        }

        private async void MatomoButton_Click(object sender, RoutedEventArgs e)
        {
            ConsentResult result = await MatomoContentDialog.ShowAsync(this.XamlRoot);
            if (result == ConsentResult.Cancelled)
                return;
            if ((result == ConsentResult.Allowed) == ViewModel.Settings.MatomoEnabled)
                return;

            _analyticsService.TrackClick(Analytics.Keys.Category.AdvancedSettingsPage, Analytics.Keys.EventName.ChangeMatomoSettings, (result == ConsentResult.Allowed) ? 1 : 0);
            if (!await ViewModel.Settings.ChangeMatomoEnabled(result == ConsentResult.Allowed))
            {
                Logger.Log(Logger.Level.Error, "Failed to change Matomo enabled setting");
                Utility.ShowUnexpectedErrorTeachingTip();
            }
        }

        private async void SentryButton_Click(object sender, RoutedEventArgs e)
        {
            ConsentResult result = await SentryContentDialog.ShowAsync(this.XamlRoot);
            if (result == ConsentResult.Cancelled)
                return;

            if ((result == ConsentResult.Allowed) == ViewModel.Settings.SentryEnabled)
                return;

            _analyticsService.TrackClick(Analytics.Keys.Category.AdvancedSettingsPage, Analytics.Keys.EventName.ChangeSentrySettings, (result == ConsentResult.Allowed) ? 1 : 0);
            if (!await ViewModel.Settings.ChangeSentryEnabled(result == ConsentResult.Allowed))
            {
                Logger.Log(Logger.Level.Error, "Failed to change Sentry enabled setting");
                Utility.ShowUnexpectedErrorTeachingTip();
            }
        }

        private async void PrivacySeeSourcesButton_Click(object sender, RoutedEventArgs e)
        {
            var control = sender as Control;
            if (control is not null)
                control.IsEnabled = false;
            _analyticsService.TrackClick(Analytics.Keys.Category.AdvancedSettingsPage, Analytics.Keys.EventName.OpenSourceCodeWeb);

            await Windows.System.Launcher.LaunchUriAsync(App.Constants.GitHub.RepoUrl);

            if (control is not null)
                control.IsEnabled = true;
        }

        private async void ProxyTypeComboBox_Changed(object sender, SelectionChangedEventArgs e)
        {
            if (!IsLoaded)
                return;

            var control = sender as Control;
            if (control is null)
            {
                Logger.Log(Logger.Level.Error, "control is null");
                return;
            }

            if (!control.IsEnabled)
                return;

            control.IsEnabled = false;
            var selectedItem = e.AddedItems.OfType<ComboBoxItem>().FirstOrDefault();
            if (selectedItem is null)
            {
                Logger.Log(Logger.Level.Error, "selectedItem is null");
                control.IsEnabled = true;
                return;
            }

            if (!Enum.TryParse<ProxyType>(selectedItem.Tag as string, out ProxyType selectedProxyType))
            {
                control.IsEnabled = true;
                Logger.Log(Logger.Level.Error, "selected item is null or invalid");
            }
            _analyticsService.TrackClick(Analytics.Keys.Category.AdvancedSettingsPage, Analytics.Keys.EventName.ChangeProxyMode);

            if (selectedProxyType == ProxyType.HTTP && ViewModel.Settings.ProxyConfig.Type != ProxyType.HTTP)
                ProxySettingsExpander.IsExpanded = true;
            else
                ProxySettingsExpander.IsExpanded = false;


            if (!await ViewModel.Settings.ChangeProxyType(selectedProxyType))
            {
                Logger.Log(Logger.Level.Error, "Failed to change proxy type");
                Utility.ShowUnexpectedErrorTeachingTip();
            }

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
            if (!ProxySettingsExpander.IsEnabled)
                return;

            ProxySaveProgressRing.Visibility = Visibility.Visible;
            ProxySettingsExpander.IsEnabled = false;
            _analyticsService.TrackClick(Analytics.Keys.Category.AdvancedSettingsPage, Analytics.Keys.EventName.ChangeProxySettings);

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

        private async void OpenDebugFolderButton_Click(object sender, RoutedEventArgs e)
        {
            _analyticsService.TrackClick(Analytics.Keys.Category.AdvancedSettingsPage, Analytics.Keys.EventName.OpenLogFolder);
            try
            {
                await Utility.OpenFolderSecurely(Logger.LogFolder);
            }
            catch (Exception ex)
            {
                Logger.Log(Logger.Level.Error, $"Failed to open debug folder: {ex.Message}");
            }
        }

        private async void EnableDebugLogsToggleSwitch_Toggled(object sender, RoutedEventArgs e)
        {
            if (sender is ToggleSwitch toggleSwitch)
            {
                if (!toggleSwitch.IsEnabled || !toggleSwitch.IsLoaded)
                    return;
                LogSettingsExpander.IsEnabled = false;
                toggleSwitch.IsEnabled = false;
                _analyticsService.TrackClick(Analytics.Keys.Category.AdvancedSettingsPage, Analytics.Keys.EventName.ChangeLogIsOn, toggleSwitch.IsOn ? 1 : 0);
                if (!await ViewModel.Settings.ChangeLogLevel(toggleSwitch.IsOn ? Logger.Level.Debug : Logger.Level.None))
                {
                    Logger.Log(Logger.Level.Error, "Failed to change log level");
                    Utility.ShowUnexpectedErrorTeachingTip();
                }
                toggleSwitch.IsEnabled = true;
                LogSettingsExpander.IsEnabled = true;
            }
        }

        private async void PurgeOldLogsToggleSwitch_Toggled(object sender, RoutedEventArgs e)
        {
            if (sender is ToggleSwitch toggleSwitch)
            {
                if (!toggleSwitch.IsEnabled || !toggleSwitch.IsLoaded)
                    return;
                LogSettingsExpander.IsEnabled = false;
                toggleSwitch.IsEnabled = false;
                _analyticsService.TrackClick(Analytics.Keys.Category.AdvancedSettingsPage, Analytics.Keys.EventName.ChangeLogPurge, toggleSwitch.IsOn ? 1 : 0);
                if (!await ViewModel.Settings.ChangePurgeOldLog(toggleSwitch.IsOn))
                {
                    Logger.Log(Logger.Level.Error, "Failed to change purge old logs setting");
                    Utility.ShowUnexpectedErrorTeachingTip();
                }
                toggleSwitch.IsEnabled = true;
                LogSettingsExpander.IsEnabled = true;
            }
        }

        private async void ExtendedLogToggleSwitch_Toggled(object sender, RoutedEventArgs e)
        {
            if (sender is ToggleSwitch toggleSwitch)
            {
                if (!toggleSwitch.IsEnabled || !toggleSwitch.IsLoaded)
                    return;
                LogSettingsExpander.IsEnabled = false;
                toggleSwitch.IsEnabled = false;

                if ((ViewModel.Settings.LogLevel == Logger.Level.Extended && toggleSwitch.IsOn) || (ViewModel.Settings.LogLevel != Logger.Level.Extended && !toggleSwitch.IsOn))
                {
                    toggleSwitch.IsEnabled = true;
                    LogSettingsExpander.IsEnabled = true;
                    return; // No change needed
                }
                _analyticsService.TrackClick(Analytics.Keys.Category.AdvancedSettingsPage, Analytics.Keys.EventName.ChangeLogVerbosity);
                if (!await ViewModel.Settings.ChangeLogLevel(toggleSwitch.IsOn ? Logger.Level.Extended : Logger.Level.Debug))
                {
                    Logger.Log(Logger.Level.Error, "Failed to change log level");
                    Utility.ShowUnexpectedErrorTeachingTip();
                }
                toggleSwitch.IsEnabled = true;
                LogSettingsExpander.IsEnabled = true;
            }
        }

        private async void LogLevelComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (!IsLoaded)
                return;
            var control = sender as Control;
            if (control is null)
            {
                Logger.Log(Logger.Level.Error, "control is null");
                return;
            }

            if (!control.IsEnabled)
                return;

            control.IsEnabled = false;

            var selectedItem = e.AddedItems.OfType<ComboBoxItem>().FirstOrDefault();
            if (selectedItem is null)
            {
                Logger.Log(Logger.Level.Error, "selectedItem is null");
                control.IsEnabled = true;
                return;
            }

            if (!Enum.TryParse<Logger.Level>(selectedItem.Tag as string, out Logger.Level selectedLevel))
            {
                Logger.Log(Logger.Level.Error, $"Selected item is null or invalid : {selectedItem.Tag}");
                control.IsEnabled = true;
                return;
            }
            _analyticsService.TrackClick(Analytics.Keys.Category.AdvancedSettingsPage, Analytics.Keys.EventName.ChangeLogVerbosity);

            if (!await ViewModel.Settings.ChangeLogLevel(selectedLevel))
            {
                Logger.Log(Logger.Level.Error, "Failed to change log level");
                Utility.ShowUnexpectedErrorTeachingTip();
            }
            control.IsEnabled = true;
        }

        private async void LanguageComboBox_Changed(object sender, SelectionChangedEventArgs e)
        {
            if (!IsLoaded)
                return;

            var control = sender as Control;
            if (control is null)
            {
                Logger.Log(Logger.Level.Error, "control is null");
                return;
            }

            if (!control.IsEnabled)
                return;

            _analyticsService.TrackClick(Analytics.Keys.Category.GeneralSettingsPage, Analytics.Keys.EventName.ChangeLanguage);

            control.IsEnabled = false;

            var selectedItem = e.AddedItems.OfType<ComboBoxItem>().FirstOrDefault();
            if (selectedItem is null)
            {
                Logger.Log(Logger.Level.Error, "selectedItem is null");
                control.IsEnabled = true;
                return;
            }

            if (!Enum.TryParse<Language>(selectedItem.Tag as string, out Language selectedLanguage))
            {
                Logger.Log(Logger.Level.Error, $"Selected item is null or invalid : {selectedItem.Tag}");
                control.IsEnabled = true;
                return;
            }

            await ViewModel.Settings.ChangeLanguage(selectedLanguage);
            Frame.BackStack.Clear(); // Prevent user from going back to a page with the old language
            Logger.Log(Logger.Level.Info, $"Language changed to {selectedLanguage}");
            control.IsEnabled = true;
        }

        private async void HelpDeskButton_Click(object sender, RoutedEventArgs e)
        {
            Control? control = sender as Control;
            if (control is not null)
                control.IsEnabled = false;
            _analyticsService.TrackClick(Analytics.Keys.Category.GeneralSettingsPage, Analytics.Keys.EventName.OpenSupportWeb);

            if (!await Windows.System.Launcher.LaunchUriAsync(App.Constants.kSuite.HelpUri))
            {
                Logger.Log(Logger.Level.Error, "Failed to launch HelpDesk URI.");
                Utility.ShowUnexpectedErrorTeachingTip();
            }

            await Task.Delay(1000);
            if (control is not null)
                control.IsEnabled = true;
        }

        private async void FeedbackButton_Click(object sender, RoutedEventArgs e)
        {
            Control? control = sender as Control;
            if (control is not null)
                control.IsEnabled = false;
            _analyticsService.TrackClick(Analytics.Keys.Category.GeneralSettingsPage, Analytics.Keys.EventName.OpenFeedbackWeb);
            if (!await kDrive.Localizer.Instance.TryLaunchUriAsync("feedbackURL"))
            {
                Logger.Log(Logger.Level.Error, "Failed to launch Feedback URI.");
                Utility.ShowUnexpectedErrorTeachingTip();
            }
            await Task.Delay(1000);
            if (control is not null)
                control.IsEnabled = true;
        }

        private async void ActivateNotificationInSystem_Button(object sender, RoutedEventArgs e)
        {

            Control? control = sender as Control;

            if (ViewModel.Settings.AppNotificationAvailability == AppNotificationAvailability.DeactivatedInSystemSettings)
            {
                await NotificationManager.OpenNotificationSystemSettings();
            }
            else
            {
                ViewModel.Settings.RefreshAppNotificationAvailability();
                return;
            }

            if (control is null)
                return;

            control.IsEnabled = false;
            // As we cannot be notified of when the user change the settings, we refresh the availability every second
            // until it is no longer deactivated in system settings or the page is unloaded.
            while (ViewModel.Settings.AppNotificationAvailability == AppNotificationAvailability.DeactivatedInSystemSettings && base.IsLoaded)
            {
                await Task.Delay(1000);
            }
            if (base.IsLoaded)
            {
                ViewModel.Settings.RefreshAppNotificationAvailability();
                control.IsEnabled = true;
            }
        }

        private void RestartAppHyperlinkButton_Click(object sender, RoutedEventArgs e)
        {
            App.RestartApplicationWindows();
            ViewModel.Settings.RestartRequiredForLanguageChange = false;
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
