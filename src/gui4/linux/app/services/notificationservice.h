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

#pragma once

#include <QObject>
#include <QString>

#include <cstdint>
#include <unordered_map>

namespace KDC {

/**
 * Desktop notifications through the freedesktop notification server (`org.freedesktop.Notifications` on the session
 * bus). A click on a notification emits `actionRequested` with the action it was shown with. Each action keeps one
 * notification on screen: a new one replaces the previous one of the same action.
 */
class NotificationService final : public QObject {
        Q_OBJECT

    public:
        enum class Action : uint8_t {
            OpenActivities,
            OpenUpdate,
        };

        explicit NotificationService(QObject *parent = nullptr);

        /** Shows `summary` and `body` as plain text: the body is escaped when the server interprets markup. */
        void show(const QString &summary, const QString &body, Action action);

    signals:
        void actionRequested(KDC::NotificationService::Action action);

    private slots:
        // D-Bus `u` arguments: the signatures must use `uint` to match the notification server signals.
        void handleActionInvoked(uint notificationId, const QString &actionKey);
        void handleNotificationClosed(uint notificationId, uint reason);

    private:
        std::unordered_map<uint32_t, Action> _actionByNotification;
        std::unordered_map<Action, uint32_t> _notificationByAction;
        bool _serverUnavailableLogged{false};
        // Until the server answers, assume it interprets the body as markup, as the common servers do.
        bool _bodyMarkup{true};
};

} // namespace KDC
