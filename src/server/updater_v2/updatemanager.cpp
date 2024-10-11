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

#include "updatemanager.h"

#include "linuxupdater.h"
#include "sparkleupdater.h"
#include "windowsupdater.h"
#include "db/parmsdb.h"
#include "log/log.h"
#include "requests/parameterscache.h"

namespace KDC {

UpdateManager::UpdateManager(QObject *parent) : QObject(parent) {
    createUpdater();

    connect(&_updateCheckTimer, &QTimer::timeout, this, &UpdateManager::slotTimerFired);

    // Note: the sparkle-updater is not an KDCUpdater
    // if (KDCUpdater *updater = qobject_cast<KDCUpdater *>(UpdaterServer::instance())) {
    //     connect(updater, &KDCUpdater::newUpdateAvailable, this, &UpdaterScheduler::updaterAnnouncement);
    //     connect(updater, &KDCUpdater::requestRestart, this, &UpdaterScheduler::requestRestart);
    // }

    // static constexpr auto checkInterval = std::chrono::hours(1);
    static constexpr auto checkTestInterval = std::chrono::seconds(15);
    _updateCheckTimer.start(std::chrono::milliseconds(checkTestInterval).count());

    // Setup callback for update state change notification
    const std::function<void(UpdateStateV2)> callback = std::bind_front(&UpdateManager::onUpdateStateChange, this);
    _updater->setStateChangeCallback(callback);
    connect(this, &UpdateManager::updateStateChanged, this, &UpdateManager::slotUpdateStateChanged, Qt::QueuedConnection);

    // At startup, do a check in any case and setup distribution channel.
    QTimer::singleShot(3000, this, [this]() { setDistributionChannel(readDistributionChannelFromDb()); });
}

void UpdateManager::startInstaller() const {
    LOG_DEBUG(Log::instance()->getLogger(), "startInstaller called!");
    _updater->startInstaller();
}

void UpdateManager::slotTimerFired() const {
    _updater->checkUpdateAvailable();
}

void UpdateManager::slotUpdateStateChanged(const UpdateStateV2 newState) {
    LOG_DEBUG(Log::instance()->getLogger(), "New update state: " << newState);
    switch (newState) {
        case UpdateStateV2::UpToDate:
        case UpdateStateV2::Checking:
        case UpdateStateV2::Downloading: {
            // Nothing to do
            break;
        }
        case UpdateStateV2::ManualUpdateAvailable: {
            emit updateAnnouncement(tr("New update available."),
                                    tr("Version %1 is available for download.").arg(_updater->versionInfo().tag.c_str()));
            break;
        }
        case UpdateStateV2::Available: {
            // A new version has been found
            _updater->onUpdateFound();
            break;
        }
        case UpdateStateV2::Ready: {
            // TODO : manage skipped version
            // The new version is ready to be installed
#if defined(__APPLE__)
            _updater->startInstaller();
#elif defined(_WIN32)
            emit showUpdateDialog();
#endif
            break;
        }
        case UpdateStateV2::DownloadError:
        case UpdateStateV2::CheckError:
        case UpdateStateV2::UpdateError: {
            // An error occurred
            break;
        }
    }
}

void UpdateManager::createUpdater() {
#if defined(__APPLE__) && defined(HAVE_SPARKLE)
    _updater = std::make_unique<SparkleUpdater>();
#elif defined(_WIN32)
    _updater = std::make_unique<WindowsUpdater>();
#else
    // the best we can do is notify about updates
    _updater = std::make_unique<LinuxUpdater>();
#endif
}

void UpdateManager::onUpdateStateChange(const UpdateStateV2 newState) {
    // Emit signal in order to run `slotUpdateStateChanged` in main thread
    emit updateStateChanged(newState);
}

DistributionChannel UpdateManager::readDistributionChannelFromDb() const {
    return ParametersCache::instance()->parameters().distributionChannel();
}

void UpdateManager::skipVersion(const std::string &skippedVersion) const {
    // TODO : write skipped version in DB
}

} // namespace KDC
