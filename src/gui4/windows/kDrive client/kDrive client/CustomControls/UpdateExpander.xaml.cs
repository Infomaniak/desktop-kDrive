using CommunityToolkit.WinUI.Controls;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class UpdateExpander : SettingsExpander
    {
        public UpdateExpander()
        {
            InitializeComponent();
        }

        public static readonly DependencyProperty SettingsProperty =
         DependencyProperty.Register(
             nameof(Settings),
             typeof(Settings),
             typeof(UpdateExpander),
             new PropertyMetadata(null, OnSettingsPropertyChanged));

        public Settings? Settings
        {
            get => (Settings?)GetValue(SettingsProperty);
            set => SetValue(SettingsProperty, value);
        }

        // The version to display in the expander either the current app version or the available update version
        public static readonly DependencyProperty DisplayedVersionProperty =
         DependencyProperty.Register(
             nameof(DisplayedVersion),
             typeof(AppVersion),
             typeof(UpdateExpander),
             new PropertyMetadata(null, OnSettingsPropertyChanged));

        public AppVersion? DisplayedVersion
        {
            get => (AppVersion?)GetValue(DisplayedVersionProperty);
            set => SetValue(DisplayedVersionProperty, value);

        }

        private static void OnSettingsPropertyChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
        {
            if (d is UpdateExpander updateExpander)
            {
                if (e.OldValue is Settings oldSettings)
                {
                    updateExpander.UnregisterSettingsEventsHandlers(oldSettings);
                }

                if (e.NewValue is Settings newSettings)
                {
                    updateExpander.RegisterSettingsEventsHandlers(newSettings);
                }
                updateExpander.Refresh();
            }
        }

        private void RegisterSettingsEventsHandlers(Settings settings)
        {
            settings.UpdateManager.PropertyChanged += OnSettingsPropertyChanged;
            settings.PropertyChanged += OnSettingsPropertyChanged;
        }

        private void UnregisterSettingsEventsHandlers(Settings settings)
        {
            settings.UpdateManager.PropertyChanged -= OnSettingsPropertyChanged;
            settings.PropertyChanged -= OnSettingsPropertyChanged;
        }

        private void OnSettingsPropertyChanged(object? sender, System.ComponentModel.PropertyChangedEventArgs e)
        {
            if (e.PropertyName == nameof(UpdateManager.AvailableUpdate) || e.PropertyName == nameof(Settings.AppVersion))
            {
                Refresh();
            }
        }

        public void Refresh()
        {
            if (Settings is null)
            {
                Logger.Log(Logger.Level.Warning, "Settings is null, this is unexpected.");
                return;
            }

            if (Settings.UpdateManager.AvailableUpdate is AppVersion updateVersion)
            {
                this.Description = Utility.GetLocalizedString("CC_UpdateExpander_UpdateAvailable_Description/TextTemplate", updateVersion.Tag);
                ExpandedTextBox.Text = Utility.GetLocalizedString("CC_UpdateExpander_UpdateAvailable_UpdateDetails/TextTemplate", updateVersion.Tag, updateVersion.BuildVersion, updateVersion.PrettyBuildDate, updateVersion.BuildDate.Year);
                DisplayedVersion = updateVersion;
            }
            else if (Settings.AppVersion is AppVersion AppVersionInfo)
            {
                this.Description = Utility.GetLocalizedString("CC_UpdateExpander_UpToDate_Description/Text");
                ExpandedTextBox.Text = Utility.GetLocalizedString("CC_UpdateExpander_UpToDate_UpdateDetails/TextTemplate", AppVersionInfo.Tag, AppVersionInfo.BuildVersion, AppVersionInfo.PrettyBuildDate, AppVersionInfo.BuildDate.Year);
                DisplayedVersion = AppVersionInfo;
            }
        }

        private async void UpdateButton_Click(object sender, RoutedEventArgs e)
        {
            if (Settings is null)
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

            if (!await Settings.UpdateManager.StartUpdate())
            {
                Logger.Log(Logger.Level.Error, "Update process failed to start.");
            }

            if (btn is not null)
            {
                btn.IsEnabled = true;
            }
        }
    }
}
