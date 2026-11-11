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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "notificationservice.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QPointer>
#include <QVariantMap>

namespace KDC {

using namespace Qt::StringLiterals;

namespace {
Q_LOGGING_CATEGORY(lcNotificationService, "gui.v4.notifications", QtInfoMsg)

const auto notificationService = u"org.freedesktop.Notifications"_s;
const auto notificationPath = u"/org/freedesktop/Notifications"_s;
const auto notificationInterface = u"org.freedesktop.Notifications"_s;
// Action invoked by a click on the notification body; servers do not show it as a button.
const auto defaultActionKey = u"default"_s;
const auto bodyMarkupCapability = u"body-markup"_s;
// Named after the installed desktop entry, see src/gui4/linux/CMakeLists.txt.
const auto applicationIconName = u"kdrive"_s;
constexpr int32_t serverDefaultTimeout = -1;
} // namespace

NotificationService::NotificationService(QObject *const parent) :
    QObject(parent) {
    auto bus = QDBusConnection::sessionBus();
    (void) bus.connect(notificationService, notificationPath, notificationInterface, u"ActionInvoked"_s, this,
                       SLOT(handleActionInvoked(uint, QString)));
    (void) bus.connect(notificationService, notificationPath, notificationInterface, u"NotificationClosed"_s, this,
                       SLOT(handleNotificationClosed(uint, uint)));

    // The body carries file names: escape it only for a server that interprets it as markup.
    const auto capabilities =
            QDBusMessage::createMethodCall(notificationService, notificationPath, notificationInterface, u"GetCapabilities"_s);
    auto *const watcher = new QDBusPendingCallWatcher(bus.asyncCall(capabilities), this);
    (void) connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *const finishedWatcher) {
        finishedWatcher->deleteLater();
        const QDBusPendingReply<QStringList> reply = *finishedWatcher;
        if (reply.isError()) {
            return;
        }

        _bodyMarkup = reply.value().contains(bodyMarkupCapability);
    });
}

void NotificationService::show(const QString &summary, const QString &body, const Action action) {
    uint32_t replacedId = 0;
    if (const auto it = _notificationByAction.find(action); it != _notificationByAction.end()) {
        replacedId = it->second;
    }

    auto message = QDBusMessage::createMethodCall(notificationService, notificationPath, notificationInterface, u"Notify"_s);
    const QVariantMap hints{{u"desktop-entry"_s, QGuiApplication::desktopFileName()}};
    message << QGuiApplication::applicationDisplayName() << replacedId << applicationIconName << summary
            << (_bodyMarkup ? body.toHtmlEscaped() : body) << QStringList{defaultActionKey, QString()} << hints
            << serverDefaultTimeout;

    auto *const watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    (void) connect(watcher, &QDBusPendingCallWatcher::finished, this,
                   [self = QPointer(this), action](QDBusPendingCallWatcher *const finishedWatcher) {
                       finishedWatcher->deleteLater();
                       if (!self) {
                           return;
                       }

                       const QDBusPendingReply<uint> reply = *finishedWatcher;
                       if (reply.isError()) {
                           // Without a notification server, every notification fails the same way: log it once.
                           if (!self->_serverUnavailableLogged) {
                               self->_serverUnavailableLogged = true;
                               qCWarning(lcNotificationService)
                                       << "Notification not shown | error:" << reply.error().name() << reply.error().message();
                           }
                           return;
                       }

                       const uint32_t notificationId = reply.value();
                       self->_actionByNotification[notificationId] = action;
                       self->_notificationByAction[action] = notificationId;
                   });
}

void NotificationService::handleActionInvoked(const uint notificationId, const QString &actionKey) {
    const auto it = _actionByNotification.find(notificationId);
    if (it == _actionByNotification.end() || actionKey != defaultActionKey) {
        return;
    }

    qCInfo(lcNotificationService) << "Notification clicked | action:" << static_cast<int32_t>(it->second);
    emit actionRequested(it->second);
}

void NotificationService::handleNotificationClosed(const uint notificationId, const uint reason) {
    (void) reason;
    const auto it = _actionByNotification.find(notificationId);
    if (it == _actionByNotification.end()) {
        return;
    }

    if (const auto shown = _notificationByAction.find(it->second);
        shown != _notificationByAction.end() && shown->second == notificationId) {
        _notificationByAction.erase(shown);
    }
    _actionByNotification.erase(it);
}

} // namespace KDC
