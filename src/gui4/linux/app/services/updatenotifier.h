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

#include <set>

namespace KDC {

class NotificationService;
class ParametersStore;
class UpdateStatusService;

/**
 * Notifies the user of an update: available for download, available while automatic updates are off, or installed and
 * waiting for a restart. Each version is notified once per state and per session, never while notifications are
 * disabled. A click opens Settings on the update button.
 */
class UpdateNotifier final : public QObject {
        Q_OBJECT

    public:
        UpdateNotifier(const UpdateStatusService &updateStatusService, const ParametersStore &parametersStore,
                       NotificationService &notificationService, QObject *parent = nullptr);

    private:
        void notifyIfNeeded();

        const UpdateStatusService &_updateStatusService;
        const ParametersStore &_parametersStore;
        NotificationService &_notificationService;
        std::set<QString> _notified; // "<state>/<version>" already notified.
};

} // namespace KDC
