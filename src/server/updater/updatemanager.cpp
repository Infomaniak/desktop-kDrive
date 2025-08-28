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

#include "updatemanager.h"

#if defined(KD_MACOS)
#include "sparkleupdater.h"
#elif defined(KD_WINDOWS)
#include "windowsupdater.h"
#else
#include "linuxupdater.h"
#endif

#include "libcommon/utility/utility.h"
#include "log/log.h"
#include "requests/parameterscache.h"

namespace KDC {

UpdateManager::UpdateManager(QObject *parent) :
    QObject(parent) {
    _currentChannel = ParametersCache::instance()->parameters().distributionChannel();

    initUpdater();

    connect(&_updateCheckTimer, &QTimer::timeout, this, &UpdateManager::slotTimerFired);

    static constexpr auto checkInterval = std::chrono::hours(1);
    _updateCheckTimer.start(std::chrono::milliseconds(checkInterval).count());

    // Setup callback for update state change notification
    const std::function<void(UpdateState)> callback = std::bind_front(&UpdateManager::onUpdateStateChanged, this);
    _updater->setStateChangeCallback(callback);
    connect(this, &UpdateManager::updateStateChanged, this, &UpdateManager::slotUpdateStateChanged, Qt::QueuedConnection);

    // At startup, do a check in any case and setup distribution channel.
    QTimer::singleShot(3000, this, [this]() { setDistributionChannel(_currentChannel); });
}

void UpdateManager::setDistributionChannel(const VersionChannel channel) {
    _currentChannel = channel;
    _updater->checkUpdateAvailable(channel);
    ParametersCache::instance()->parameters().setDistributionChannel(channel);
    ParametersCache::instance()->save();
}

void UpdateManager::startInstaller() const {
    LOG_DEBUG(Log::instance()->getLogger(), "startInstaller called!");

    // Cleanup skipped version
    _updater->unskipVersion();

    _updater->startInstaller();
}

void UpdateManager::slotTimerFired() const {
    _updater->checkUpdateAvailable(_currentChannel);
}

void UpdateManager::slotUpdateStateChanged(const UpdateState newState) {
    LOG_DEBUG(Log::instance()->getLogger(), "New update state: " << newState);
    switch (newState) {
        case UpdateState::UpToDate:
        case UpdateState::Checking:
        case UpdateState::Downloading: {
            // Nothing to do
            break;
        }
        case UpdateState::ManualUpdateAvailable: {
            emit updateAnnouncement(
                    tr("New update available."),
                    tr("Version %1 is available for download.").arg(_updater->versionInfo(_currentChannel).tag.c_str()));
            break;
        }
        case UpdateState::Available: {
            // A new version has been found
            _updater->onUpdateFound();
            break;
        }
        case UpdateState::Ready: {
            if (AbstractUpdater::isVersionSkipped(_updater->versionInfo(_currentChannel).fullVersion())) break;
            // The new version is ready to be installed
#if defined(KD_WINDOWS)
            emit showUpdateDialog();
#endif
            break;
        }
        case UpdateState::Unknown:
        case UpdateState::DownloadError:
        case UpdateState::CheckError:
        case UpdateState::UpdateError: {
            // An error occurred
            break;
        }
        case UpdateState::EnumEnd: {
            assert(false && "Invalid enum value in switch statement.");
        }
    }
}

void UpdateManager::initUpdater() {
    _updater = createUpdater();
}

void UpdateManager::onUpdateStateChanged(const UpdateState newState) {
    // Emit signal in order to run `slotUpdateStateChanged` in main thread
    emit updateStateChanged(newState);
}

} // namespace KDC
