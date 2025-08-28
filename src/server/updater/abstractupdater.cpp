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

#include "abstractupdater.h"

#include "dummyupdater.h"
#include "log/log.h"
#include "requests/parameterscache.h"
#if defined(KD_MACOS)
#include "sparkleupdater.h"
#elif defined(KD_WINDOWS)
#include "windowsupdater.h"
#else
#include "linuxupdater.h"
#endif

namespace KDC {

AbstractUpdater::AbstractUpdater() :
    _updateChecker(std::make_shared<UpdateChecker>()) {
    const std::function callback = [this] { onAppVersionReceived(); };
    _updateChecker->setCallback(callback);
}

AbstractUpdater::AbstractUpdater(const std::shared_ptr<UpdateChecker> &updateChecker) :
    _updateChecker(updateChecker) {
    const std::function callback = [this] { onAppVersionReceived(); };
    _updateChecker->setCallback(callback);
}

ExitCode AbstractUpdater::checkUpdateAvailable(const VersionChannel currentChannel, UniqueId *id /*= nullptr*/) {
    _currentChannel = currentChannel;
    setState(UpdateState::Checking);
    return _updateChecker->checkUpdateAvailability(id);
}

void AbstractUpdater::setStateChangeCallback(const std::function<void(UpdateState)> &stateChangeCallback) {
    LOG_INFO(Log::instance()->getLogger(), "Set state change callback");
    _stateChangeCallback = stateChangeCallback;
}

void AbstractUpdater::onAppVersionReceived() {
    if (!_updateChecker->isVersionReceived()) {
        setState(UpdateState::CheckError);
        LOG_WARN(Log::instance()->getLogger(), "Error while retrieving latest app version");
        return;
    }

    const VersionInfo &versionInfo = _updateChecker->versionInfo(_currentChannel);
    if (!versionInfo.isValid()) {
        LOG_INFO(Log::instance()->getLogger(), "No valid update info retrieved for distribution channel: " << _currentChannel);
        setState(UpdateState::UpToDate);
    }

    const bool newVersionAvailable = CommonUtility::isVersionLower(CommonUtility::currentVersion(), versionInfo.fullVersion());
    setState(newVersionAvailable ? UpdateState::Available : UpdateState::UpToDate);
    if (newVersionAvailable) {
        LOG_INFO(Log::instance()->getLogger(), "New app version available");
    } else {
        LOG_INFO(Log::instance()->getLogger(), "App version is up to date");
    }

    sentry::Handler::instance()->setDistributionChannel(currentVersionChannel());
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

    const auto seenVerison = ParametersCache::instance()->parameters().seenVersion();
    if (seenVerison.empty()) return false;

    if (seenVerison == version || CommonUtility::isVersionLower(version, seenVerison)) {
        LOG_INFO(Log::instance()->getLogger(), "Version " << seenVerison << " has been skipped.");
        return true;
    }
    return false;
}

VersionChannel AbstractUpdater::currentVersionChannel() const {
    const std::unordered_map<VersionChannel, VersionInfo> allVersions = _updateChecker->versionsInfo();
    if (allVersions.empty()) return VersionChannel::Unknown;
    std::string currentVersion = getCurrentVersion();
    if (allVersions.contains(VersionChannel::Prod) && allVersions.at(VersionChannel::Prod).fullVersion() == currentVersion) {
        return VersionChannel::Prod;
    }

    if (allVersions.contains(VersionChannel::Next) && allVersions.at(VersionChannel::Next).fullVersion() == currentVersion) {
        return VersionChannel::Next;
    }

    if (allVersions.contains(VersionChannel::Beta) && allVersions.at(VersionChannel::Beta).fullVersion() == currentVersion) {
        return VersionChannel::Beta;
    }

    if (allVersions.contains(VersionChannel::Internal) &&
        allVersions.at(VersionChannel::Internal).fullVersion() == currentVersion) {
        return VersionChannel::Internal;
    }

    if (allVersions.contains(VersionChannel::Prod) &&
        CommonUtility::isVersionLower(currentVersion, allVersions.at(VersionChannel::Prod).fullVersion())) {
        return VersionChannel::Legacy;
    }

    return VersionChannel::Unknown;
}

void AbstractUpdater::setState(const UpdateState newState) {
    if (_state != newState) {
        LOG_INFO(Log::instance()->getLogger(), "Update state changed to: " << newState);
        _state = newState;
        if (_stateChangeCallback) _stateChangeCallback(_state);
    }
}

std::unique_ptr<AbstractUpdater> createUpdater() {
    // if (ParametersCache::instance()->parameters().updateDeactivated()) {
    //     LOG_INFO(Log::instance()->getLogger(), "Update are deactivated");
    //     return std::make_unique<DummyUpdater>();
    // }
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
