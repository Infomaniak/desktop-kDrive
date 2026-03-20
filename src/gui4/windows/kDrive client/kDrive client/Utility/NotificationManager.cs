using Infomaniak.kDrive.ViewModels;
using Microsoft.Diagnostics.Utilities;
using Microsoft.Windows.AppNotifications;
using Microsoft.Windows.AppNotifications.Builder;
using System;
using System.Threading.Tasks;
using Windows.System;

namespace Infomaniak.kDrive
{
    public enum AppNotificationAvailability
    {
        Available,
        DeactivatedInSystemSettings,
        NotSupportedByOS
    }
    internal class NotificationManager
    {
        private bool m_isRegistered;

        public AppNotificationAvailability Availability
        {
            get
            {
                switch (AppNotificationManager.Default.Setting)
                {
                    case AppNotificationSetting.Enabled:
                        return AppNotificationAvailability.Available;
                    case AppNotificationSetting.DisabledForApplication:
                    case AppNotificationSetting.DisabledForUser:
                    case AppNotificationSetting.DisabledByGroupPolicy:
                        return AppNotificationAvailability.DeactivatedInSystemSettings;
                    default:
                        return AppNotificationAvailability.NotSupportedByOS;
                }
            }
        }

        public void Init()
        {
            if (Availability != AppNotificationAvailability.Available)
            {
                Logger.Log(Logger.Level.Warning, $"App notifications are not available: {Availability}");
                return;
            }

            // To ensure all Notification handling happens in this process instance, register for
            // NotificationInvoked before calling Register(). Without this a new process will
            // be launched to handle the notification.
            if (!m_isRegistered)
            {
                Logger.Log(Logger.Level.Info, "Registering for notifications");
                AppNotificationManager notificationManager = AppNotificationManager.Default;
                notificationManager.NotificationInvoked += OnNotificationInvoked;
                notificationManager.Register();
                m_isRegistered = true;

            }
            else
            {
                Logger.Log(Logger.Level.Warning, "Already registered for notifications");
            }


        }

        private void OnNotificationInvoked(AppNotificationManager sender, AppNotificationActivatedEventArgs args)
        {
            Logger.Log(Logger.Level.Info, $"Notification activated with arguments: {args.Argument}");
            AppModel.UIThreadDispatcher.TryEnqueue(() => (App.Current as App)?.CurrentWindow?.AppWindow.Show());
        }

        public async Task UnregisterAsync()
        {
            if (m_isRegistered)
            {
                await AppNotificationManager.Default.RemoveAllAsync();
                AppNotificationManager.Default.Unregister();
                m_isRegistered = false;
            }
        }

        public uint ShowNotification(string title, string message)
        {
            if (!m_isRegistered)
            {
                Init(); // The Notifications might have been enabled after the app start in system settings
                if (!m_isRegistered)
                {
                    Logger.Log(Logger.Level.Warning, "Attempted to show a notification while not registered. Call Init() first.");
                    return 0;
                }
            }

            var appNotification = new AppNotificationBuilder()
            .AddText(title)
            .AddText(message)
            .BuildNotification();
            appNotification.ExpiresOnReboot = true;
            AppNotificationManager.Default.Show(appNotification);
            if (appNotification.Id == 0)
                Logger.Log(Logger.Level.Error, "Failed to show notification.");

            return appNotification.Id; // return the Id of the notification (0 if it failed to send)
        }

        public static async Task OpenNotificationSystemSettings()
        {
            try
            {
                await Launcher.LaunchUriAsync(new Uri("ms-settings:notifications"));
            }
            catch (Exception ex)
            {
                Logger.Log(Logger.Level.Error, $"Failed to open notification settings: {ex.Message}");
            }
        }
    }
}
