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

#include "systray.h"
#include "config.h"
#include "libcommon/theme/theme.h"
#include "libcommon/utility/utility.h"

#ifdef USE_FDO_NOTIFICATIONS
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#define NOTIFICATIONS_SERVICE "org.freedesktop.Notifications"
#define NOTIFICATIONS_PATH "/org/freedesktop/Notifications"
#define NOTIFICATIONS_IFACE "org.freedesktop.Notifications"
#endif

namespace KDC {

#ifdef Q_OS_OSX
bool isOsXNotificationEnabled();
void osxSendNotification(const QString &title, const QString &message);
#endif

void Systray::showMessage(const QString &title, const QString &message, MessageIcon icon, int millisecondsTimeoutHint) {
#ifdef USE_FDO_NOTIFICATIONS
    KDC::SyncPath iconPath = KDC::CommonUtility::getAppWorkingDir() / "../../kdrive-win.png";
    QString iconPathURI = SyncName2QStr(iconPath.native());

    if (QDBusInterface(NOTIFICATIONS_SERVICE, NOTIFICATIONS_PATH, NOTIFICATIONS_IFACE).isValid()) {
        QList<QVariant> args = QList<QVariant>() << APPLICATION_NAME // Application Name
                                                 << quint32(0) // Replaces ID
                                                 << iconPathURI // Notification Icon
                                                 << title // Summary
                                                 << message // Body
                                                 << QStringList() // Actions
                                                 << QVariantMap() // Hints
                                                 << qint32(-1); // Expiration Timeout

        QDBusMessage method =
                QDBusMessage::createMethodCall(NOTIFICATIONS_SERVICE, NOTIFICATIONS_PATH, NOTIFICATIONS_IFACE, "Notify");
        method.setArguments(args);
        QDBusConnection::sessionBus().asyncCall(method);
    } else
#endif
#ifdef Q_OS_OSX
            if (isOsXNotificationEnabled()) {
        osxSendNotification(title, message);
    } else
#endif
    {
        QSystemTrayIcon::showMessage(title, message, icon, millisecondsTimeoutHint);
    }
}

void Systray::setToolTip(const QString &tip) {
    QSystemTrayIcon::setToolTip(tip);
}

} // namespace KDC
