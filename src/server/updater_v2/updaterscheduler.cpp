
/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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

#include "updaterscheduler.h"

namespace KDC {

UpdaterScheduler::UpdaterScheduler(QObject *parent) : QObject(parent) {
    connect(&_updateCheckTimer, &QTimer::timeout, this, &UpdaterScheduler::slotTimerFired);

    // Note: the sparkle-updater is not an KDCUpdater
    // if (KDCUpdater *updater = qobject_cast<KDCUpdater *>(UpdaterServer::instance())) {
    //     connect(updater, &KDCUpdater::newUpdateAvailable, this, &UpdaterScheduler::updaterAnnouncement);
    //     connect(updater, &KDCUpdater::requestRestart, this, &UpdaterScheduler::requestRestart);
    // }

    // at startup, do a check in any case.
    QTimer::singleShot(3000, this, &UpdaterScheduler::slotTimerFired);

    auto checkInterval = std::chrono::hours(1);
    _updateCheckTimer.start(std::chrono::milliseconds(checkInterval).count());
}

void UpdaterScheduler::slotTimerFired() {
    // UpdaterServer *updater = UpdaterServer::instance();
    // if (updater) {
    //     updater->backgroundCheckForUpdate();
    // }
}

} // namespace KDC
