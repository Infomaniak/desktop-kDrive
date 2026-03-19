using Microsoft.Diagnostics.Utilities;
using Microsoft.Windows.AppNotifications;
using Microsoft.Windows.AppNotifications.Builder;
using System;

namespace Infomaniak.kDrive
{
    internal class NotificationManager
    {
        private bool m_isRegistered;

        ~NotificationManager()
        {
            Unregister();
        }

        public void Init()
        {
            // To ensure all Notification handling happens in this process instance, register for
            // NotificationInvoked before calling Register(). Without this a new process will
            // be launched to handle the notification.
            if (!m_isRegistered)
            {
                Logger.Log(Logger.Level.Info, "Registering for notifications");
                AppNotificationManager notificationManager = AppNotificationManager.Default;
                notificationManager.NotificationInvoked += OnNotificationInvoked;
                notificationManager.Register(); m_isRegistered = true;

            }
            else
                Logger.Log(Logger.Level.Warning, "Already registered for notifications");

        }

        private void OnNotificationInvoked(AppNotificationManager sender, AppNotificationActivatedEventArgs args)
        {
            Logger.Log(Logger.Level.Info, $"Notification activated with arguments: {args.Argument}");
        }

        public void Unregister()
        {
            if (m_isRegistered)
            {
                AppNotificationManager.Default.Unregister();
                m_isRegistered = false;
            }
        }

        public uint ShowNotification(string title, string message)
        {
            var appNotification = new AppNotificationBuilder()
                .AddText(title)
                .AddText(message)
                .BuildNotification();
            AppNotificationManager.Default.Show(appNotification);
            return appNotification.Id; // return the Id of the notification (0 if it failed to send)
        }
    }
}
