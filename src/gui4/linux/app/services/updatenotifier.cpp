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

#include "updatenotifier.h"

#include "app/cache/parametersstore.h"
#include "app/services/notificationservice.h"
#include "app/services/updatestatusservice.h"

#include <QGuiApplication>
#include <QLoggingCategory>

namespace KDC {

using namespace Qt::StringLiterals;

namespace {
Q_LOGGING_CATEGORY(lcUpdateNotifier, "gui.v4.updatenotifier", QtInfoMsg)
} // namespace

UpdateNotifier::UpdateNotifier(const UpdateStatusService &updateStatusService, const ParametersStore &parametersStore,
                               NotificationService &notificationService, QObject *const parent) :
    QObject(parent),
    _updateStatusService(updateStatusService),
    _parametersStore(parametersStore),
    _notificationService(notificationService) {
    // The version arrives after the state: both changes are observed through `changed`.
    (void) connect(&updateStatusService, &UpdateStatusService::changed, this, &UpdateNotifier::notifyIfNeeded);
    // The parameters can arrive after the update state, and decide whether the notification is shown.
    (void) connect(&parametersStore, &ParametersStore::parametersChanged, this, &UpdateNotifier::notifyIfNeeded);
}

void UpdateNotifier::notifyIfNeeded() {
    const auto version = _updateStatusService.announcedRelease();
    const auto parameters = _parametersStore.parameters();
    if (version.isEmpty() || !parameters) {
        return;
    }

    // Text built in C++: a notification has no QML owner.
    QString body;
    switch (_updateStatusService.state()) {
        case UpdateState::ManualUpdateAvailable:
            body = qtTrId("updateAvailable").arg(version);
            break;
        case UpdateState::Available:
            // With automatic updates, the server prepares the update: only its installation is notified.
            if (!parameters->automaticUpdates()) {
                body = qtTrId("updateAvailable").arg(version);
            }
            break;
        case UpdateState::Ready:
            body = qtTrId("updateInstalled").arg(version);
            break;
        default:
            break;
    }

    if (body.isEmpty() || parameters->notificationsDisabled() == NotificationsDisabled::Always) {
        return;
    }

    // The full version tells apart two builds of a release, which can share the same label.
    const auto key = u"%1/%2"_s.arg(static_cast<int32_t>(_updateStatusService.state()))
                             .arg(QString::fromStdString(_updateStatusService.version()->fullVersion()));
    if (!_notified.insert(key).second) {
        return;
    }

    qCInfo(lcUpdateNotifier) << "Update notification shown | state:" << _updateStatusService.state() << "| version:" << version;
    _notificationService.show(QGuiApplication::applicationDisplayName(), body, NotificationService::Action::OpenUpdate);
}

} // namespace KDC
