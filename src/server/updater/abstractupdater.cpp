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

#include "libcommon/utility/utility.h"
#include "log/log.h"
#include "requests/parameterscache.h"
#if defined(__APPLE__)
#include "sparkleupdater.h"
#endif

namespace KDC {

AbstractUpdater::AbstractUpdater() : _updateChecker(std::make_shared<UpdateChecker>()) {
    const std::function callback = [this] { onAppVersionReceived(); };
    _updateChecker->setCallback(callback);
}

AbstractUpdater::AbstractUpdater(const std::shared_ptr<UpdateChecker>& updateChecker) {
    _updateChecker = updateChecker;
    const std::function callback = [this] { onAppVersionReceived(); };
    _updateChecker->setCallback(callback);
}

ExitCode AbstractUpdater::checkUpdateAvailable(const DistributionChannel currentChannel, UniqueId* id /*= nullptr*/) {
    _currentChannel = currentChannel;
    setState(UpdateState::Checking);
    return _updateChecker->checkUpdateAvailability(id);
}

void AbstractUpdater::setStateChangeCallback(const std::function<void(UpdateState)>& stateChangeCallback) {
    LOG_INFO(Log::instance()->getLogger(), "Set state change callback");
    _stateChangeCallback = stateChangeCallback;
}

void AbstractUpdater::onAppVersionReceived() {
    if (!_updateChecker->isVersionReceived()) {
        setState(UpdateState::CheckError);
        LOG_WARN(Log::instance()->getLogger(), "Error while retrieving latest app version");
        return;
    }

    const VersionInfo& versionInfo = _updateChecker->versionInfo(_currentChannel);
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

void AbstractUpdater::skipVersion(const std::string& skippedVersion) {
    ParametersCache::instance()->parameters().setSeenVersion(skippedVersion);
    ParametersCache::instance()->save();
}

void AbstractUpdater::unskipVersion() {
    if (!ParametersCache::instance()->parameters().seenVersion().empty()) {
        ParametersCache::instance()->parameters().setSeenVersion("");
        ParametersCache::instance()->save();
    }
#if defined(__APPLE__)
    SparkleUpdater::unskipVersion();
#endif
}

bool AbstractUpdater::isVersionSkipped(const std::string& version) {
    if (version.empty()) return false;

    const auto seenVerison = ParametersCache::instance()->parameters().seenVersion();
    if (seenVerison.empty()) return false;

    if (seenVerison == version || CommonUtility::isVersionLower(version, seenVerison)) {
        LOG_INFO(Log::instance()->getLogger(), "Version " << seenVerison.c_str() << " has been skipped.");
        return true;
    }
    return false;
}

DistributionChannel AbstractUpdater::currentVersionChannel() const {
    const std::unordered_map<DistributionChannel, VersionInfo> allVersions = _updateChecker->versionsInfo();
    if (allVersions.empty()) return DistributionChannel::Unknown;
    std::string currentVersion = CommonUtility::currentVersion();
    if (allVersions.contains(DistributionChannel::Prod) &&
        allVersions.at(DistributionChannel::Prod).fullVersion() == currentVersion) {
        return DistributionChannel::Prod;
    }

    if (allVersions.contains(DistributionChannel::Next) &&
        allVersions.at(DistributionChannel::Next).fullVersion() == currentVersion) {
        return DistributionChannel::Next;
    }

    if (allVersions.contains(DistributionChannel::Beta) &&
        allVersions.at(DistributionChannel::Beta).fullVersion() == currentVersion) {
        return DistributionChannel::Beta;
    }

    if (allVersions.contains(DistributionChannel::Internal) &&
        allVersions.at(DistributionChannel::Internal).fullVersion() == currentVersion) {
        return DistributionChannel::Internal;
    }

    if (allVersions.contains(DistributionChannel::Prod) &&
        CommonUtility::isVersionLower(currentVersion, allVersions.at(DistributionChannel::Prod).fullVersion())) {
        return DistributionChannel::Legacy;
    }

    return DistributionChannel::Unknown;
}

void AbstractUpdater::setState(const UpdateState newState) {
    if (_state != newState) {
        LOG_INFO(Log::instance()->getLogger(), "Update state changed to: " << newState);
        _state = newState;
        if (_stateChangeCallback) _stateChangeCallback(_state);
    }
}

} // namespace KDC
