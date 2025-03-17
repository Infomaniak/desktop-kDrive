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

#if defined(__APPLE__)
#include "sparkleupdater.h"
#elif defined(_WIN32)
#include "windowsupdater.h"
#else
#include "linuxupdater.h"
#endif

#include "libcommon/utility/utility.h"
#include "log/log.h"
#include "requests/parameterscache.h"
#include "updater.h"

namespace KDC {


std::shared_ptr<UpdateManager> UpdateManager::_instance;

UpdateManager::UpdateManager(QObject *parent) : QObject(parent) {
    _currentChannel = ParametersCache::instance()->parameters().distributionChannel();

    connect(&_updateCheckTimer, &QTimer::timeout, this, &UpdateManager::slotTimerFired);

    static constexpr auto checkInterval = std::chrono::hours(1);
    _updateCheckTimer.start(std::chrono::milliseconds(checkInterval).count());

    // Setup callback for update state change notification
    const std::function<void(UpdateState)> callback = std::bind_front(&UpdateManager::onUpdateStateChanged, this);
    Updater::instance()->setStateChangeCallback(callback);
    connect(this, &UpdateManager::updateStateChanged, this, &UpdateManager::slotUpdateStateChanged, Qt::QueuedConnection);

    // At startup, do a check in any case and setup distribution channel.
    QTimer::singleShot(3000, this, [this]() { setDistributionChannel(_currentChannel); });
}

std::shared_ptr<UpdateManager> UpdateManager::instance() {
    if (_instance == nullptr) _instance.reset(new UpdateManager());

    return _instance;
}

const VersionInfo &UpdateManager::versionInfo(const VersionChannel channel) const {
    return Updater::instance()->versionInfo(channel == VersionChannel::Unknown ? _currentChannel : channel);
}

const UpdateState &UpdateManager::state() const {
    return Updater::instance()->state();
}

void UpdateManager::setQuitCallback(const std::function<void()> &quitCallback) const {
    Updater::instance()->setQuitCallback(quitCallback);
}

void UpdateManager::setDistributionChannel(const VersionChannel channel) {
    _currentChannel = channel;
    Updater::instance()->checkUpdateAvailable(channel);
    ParametersCache::instance()->parameters().setDistributionChannel(channel);
    ParametersCache::instance()->save();
}

void UpdateManager::startInstaller() const {
    LOG_DEBUG(Log::instance()->getLogger(), "startInstaller called!");

    // Cleanup skipped version
    Updater::instance()->unskipVersion();

    Updater::instance()->startInstaller();
}

void UpdateManager::slotTimerFired() const {
    Updater::instance()->checkUpdateAvailable(_currentChannel);
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
            emit updateAnnouncement(tr("New update available."),
                                    tr("Version %1 is available for download.")
                                            .arg(Updater::instance()->versionInfo(_currentChannel).tag.c_str()));
            break;
        }
        case UpdateState::Available: {
            // A new version has been found
            Updater::instance()->onUpdateFound();
            break;
        }
        case UpdateState::Ready: {
            if (Updater::isVersionSkipped(Updater::instance()->versionInfo(_currentChannel).fullVersion())) break;
                // The new version is ready to be installed
#if defined(_WIN32)
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
    }
}

void UpdateManager::onUpdateStateChanged(const UpdateState newState) {
    // Emit signal in order to run `slotUpdateStateChanged` in main thread
    emit updateStateChanged(newState);
}

} // namespace KDC
