
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

#include "updatemanager.h"
#include "log/log.h"

namespace KDC {

UpdaterScheduler::UpdaterScheduler(QObject *parent) : QObject(parent) {
    connect(&_updateCheckTimer, &QTimer::timeout, this, &UpdaterScheduler::slotTimerFired);

    // Note: the sparkle-updater is not an KDCUpdater
    // if (KDCUpdater *updater = qobject_cast<KDCUpdater *>(UpdaterServer::instance())) {
    //     connect(updater, &KDCUpdater::newUpdateAvailable, this, &UpdaterScheduler::updaterAnnouncement);
    //     connect(updater, &KDCUpdater::requestRestart, this, &UpdaterScheduler::requestRestart);
    // }

    // At startup, do a check in any case.
    QTimer::singleShot(3000, this, &UpdaterScheduler::slotTimerFired);

    static constexpr auto checkInterval = std::chrono::hours(1);
    _updateCheckTimer.start(std::chrono::milliseconds(checkInterval).count());

    const std::function<void(UpdateStateV2)> callback = std::bind_front(&UpdaterScheduler::onUpdateStateChange, this);
    UpdateManager::instance()->setStateChangeCallback(callback);

    connect(this, &UpdaterScheduler::updateStateChanged, this, &UpdaterScheduler::slotUpdateStateChanged, Qt::QueuedConnection);
}

void UpdaterScheduler::slotTimerFired() const {
    UpdateManager::instance()->checkUpdateAvailable();
}

void UpdaterScheduler::slotUpdateStateChanged(const UpdateStateV2 newState) const {
    LOG_DEBUG(Log::instance()->getLogger(), "New update state: " << newState);
    switch (newState) {
        case UpdateStateV2::UpToDate:
        case UpdateStateV2::Checking:
        case UpdateStateV2::Downloading:
            // Nothing to do
            break;
        case UpdateStateV2::Available:
            // A new version has been found
            UpdateManager::instance()->onUpdateFound();
            break;
        case UpdateStateV2::Ready:
            // The new version is ready to be installed
            UpdateManager::instance()->startInstaller();
            break;
        case UpdateStateV2::Error:
            // An error occured
            break;
    }
}

void UpdaterScheduler::onUpdateStateChange(const UpdateStateV2 newState) {
    emit updateStateChanged(newState);
}

} // namespace KDC
