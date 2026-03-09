using CommunityToolkit.WinUI.Controls;
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
        private readonly AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        public AppModel ViewModel => _viewModel;

        public UpdateExpander()
        {
            InitializeComponent();

        }
        private void UpdateExpander_Loaded(object sender, RoutedEventArgs e)
        {
            RegisterPropertyChangedHandlers();
            UpdateInternalChannelComboBoxVisibility();
        }

        private void UpdateExpander_Unloaded(object sender, RoutedEventArgs e)
        {
            UnregisterPropertyChangedHandlers();
        }

        private bool IsStaffUserConnected()
        {
            return ViewModel.Users.Any(u => u.IsStaff && u.IsConnected);
        }

        private void UpdateInternalChannelComboBoxVisibility()
        {
            UpdateChannelComboBox_Internal.Visibility =
                IsStaffUserConnected() ? Visibility.Visible : Visibility.Collapsed;
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

        // The version to display in the expander either the current app version or the available update version
        public static readonly DependencyProperty DisplayedVersionProperty =
         DependencyProperty.Register(
             nameof(DisplayedVersion),
             typeof(AppVersion),
             typeof(UpdateExpander),
             new PropertyMetadata(null));

        public AppVersion? DisplayedVersion
        {
            get => (AppVersion?)GetValue(DisplayedVersionProperty);
            set => SetValue(DisplayedVersionProperty, value);

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
                base.Description = Localizer.Instance.GetString("updateAvailable", updateVersion.Tag);
                DisplayedVersion = updateVersion;
            }
            else if (ViewModel.Settings.AppVersion is AppVersion AppVersionInfo)
            {
                base.Description = Localizer.Instance.GetString("appUpToDate");
                DisplayedVersion = AppVersionInfo;
            }
            UpdateInternalChannelComboBoxVisibility();
        }

        private async void UpdateChannel_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (!IsLoaded)
                return;

            if (sender is ComboBox comboBox && comboBox.SelectedItem is ComboBoxItem selectedItem)
            {
                string? channelString = selectedItem.Tag as string;
                if (Enum.TryParse<VersionChannel>(channelString, out VersionChannel selectedChannel))
                {
                    if (!await ViewModel.Settings.UpdateManager.ChangeChannel(selectedChannel))
                    {
                        Logger.Log(Logger.Level.Error, $"Failed to change update channel to {selectedChannel}");
                        Utility.ShowUnexpectedErrorTeachingTip();
                        return;
                    }
                    Logger.Log(Logger.Level.Info, $"Update channel changed to {selectedChannel}");
                }
                else
                {
                    Logger.Log(Logger.Level.Error, $"Invalid update channel selected: {channelString}");
                }
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
                if (!await ViewModel.Settings.UpdateManager.ChangeAutoUpdate(toggleSwitch.IsOn))
                {
                    Logger.Log(Logger.Level.Error, "Failed to change auto-update setting.");
                    Utility.ShowUnexpectedErrorTeachingTip();
                }
                toggleSwitch.IsEnabled = true;
            }
        }
    }
}
