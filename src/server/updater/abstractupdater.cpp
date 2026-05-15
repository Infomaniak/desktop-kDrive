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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "abstractupdater.h"

#include "libcommonserver/log/log.h"
#include "requests/parameterscache.h"
#if defined(KD_MACOS)
#include "sparkleupdater.h"
#elif defined(KD_WINDOWS)
#include "windowsupdater.h"
#else
#include "linuxupdater.h"
#endif

namespace KDC {

AbstractUpdater::AbstractUpdater() {
    _versionRetriever = std::make_shared<VersionRetriever>();
    const std::function callback = [this] { onAppVersionReceived(); };
    _versionRetriever->setCallback(callback);
}

ExitCode AbstractUpdater::checkUpdateAvailable(const DistributionChannel currentChannel, UniqueId *id /*= nullptr*/) {
    setState(UpdateState::Checking);
    return _versionRetriever->retrieveVersion(currentChannel, id);
}

void AbstractUpdater::setStateChangeCallback(const std::function<void(UpdateState)> &stateChangeCallback) {
    LOG_INFO(Log::instance()->getLogger(), "Set state change callback");
    _stateChangeCallback = stateChangeCallback;
}

void AbstractUpdater::onAppVersionReceived() {
    _appShouldBeBlocked = false;

    if (!_versionRetriever->isVersionReceived()) {
        setState(UpdateState::CheckError);
        LOG_WARN(Log::instance()->getLogger(), "Error while retrieving latest app version");
        return;
    }

    const auto &versionInfo = _versionRetriever->versionInfo();
    if (!versionInfo.isValid()) {
        LOG_WARN(Log::instance()->getLogger(), "No valid update info retrieved!");
        setState(UpdateState::UpToDate);
        return;
    }

    if (!checkMinOsVersion(versionInfo.minOsVersion)) {
        setState(UpdateState::UpToDate);
        return;
    }

    // Check if app should be blocked
    if (CommonUtility::isVersionLower(CommonUtility::currentVersion(), versionInfo.minAppVersion)) {
        LOG_WARN(Log::instance()->getLogger(),
                 "The current version needs to be updated. Current version: " << CommonUtility::currentVersion()
                                                                              << ", min version: " << versionInfo.minAppVersion);
        _appShouldBeBlocked = true;
    }

    const bool newVersionAvailable = CommonUtility::isVersionLower(CommonUtility::currentVersion(), versionInfo.fullVersion());
    setState(newVersionAvailable ? UpdateState::Available : UpdateState::UpToDate);
    if (newVersionAvailable) {
        LOG_INFO(Log::instance()->getLogger(), "New app version available");
    } else {
        LOG_INFO(Log::instance()->getLogger(), "App version is up to date");
    }
}

bool AbstractUpdater::checkMinOsVersion(const std::string &minOsVersion) const {
    if (const auto currentOsVersion = CommonUtility::osVersion(); CommonUtility::isVersionLower(currentOsVersion, minOsVersion)) {
        LOG_WARN(Log::instance()->getLogger(), "OS version not supported, the update is ignored. Current OS version: "
                                                       << CommonUtility::osVersion() << ", min OS version: " << minOsVersion);
        return false;
    }
    return true;
}

void AbstractUpdater::skipVersion(const std::string &skippedVersion) {
    ParametersCache::instance()->parameters().setSeenVersion(skippedVersion);
    ParametersCache::instance()->save();
}

void AbstractUpdater::unskipVersion() {
    if (!ParametersCache::instance()->parameters().seenVersion().empty()) {
        ParametersCache::instance()->parameters().setSeenVersion("");
        ParametersCache::instance()->save();
    }
}

bool AbstractUpdater::isVersionSkipped(const std::string &version) {
    if (version.empty()) return false;

    const auto seenVersion = ParametersCache::instance()->parameters().seenVersion();
    if (seenVersion.empty()) return false;

    if (seenVersion == version || CommonUtility::isVersionLower(version, seenVersion)) {
        LOG_INFO(Log::instance()->getLogger(), "Version " << seenVersion << " has been skipped.");
        return true;
    }

    return false;
}

void AbstractUpdater::setState(const UpdateState newState) {
    if (_state != newState) {
        LOG_INFO(Log::instance()->getLogger(), "Update state changed to: " << newState);
        _state = newState;
        if (_stateChangeCallback) _stateChangeCallback(_state);
    }
}

std::unique_ptr<AbstractUpdater> createUpdater() {
#if defined(KD_MACOS)
    return std::make_unique<SparkleUpdater>();
#elif defined(KD_WINDOWS)
    return std::make_unique<WindowsUpdater>();
#else
    // the best we can do is notify about updates
    return std::make_unique<LinuxUpdater>();
#endif
}

} // namespace KDC
