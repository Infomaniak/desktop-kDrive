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

#include "updatechecker.h"

#include "db/parmsdb.h"
#include "jobs/jobmanager.h"
#include "jobs/network/abstractnetworkjob.h"
#include "jobs/network/getappversionjob.h"
#include "libcommon/utility/utility.h"
#include "log/log.h"
#include "utility/utility.h"

namespace KDC {

ExitCode UpdateChecker::checkUpdateAvailability(UniqueId *id /*= nullptr*/) {
    std::shared_ptr<AbstractNetworkJob> job;
    if (const auto exitCode = generateGetAppVersionJob(job); exitCode != ExitCode::Ok) return exitCode;
    if (id) *id = job->jobId();

    LOG_INFO(Log::instance()->getLogger(), "Looking for new app version...");

    const std::function<void(UniqueId)> callback = std::bind_front(&UpdateChecker::versionInfoReceived, this);
    JobManager::instance()->queueAsyncJob(job, Poco::Thread::PRIO_NORMAL, callback);
    return ExitCode::Ok;
}

void UpdateChecker::setCallback(const std::function<void()> &callback) {
    LOG_INFO(Log::instance()->getLogger(), "Set callback");
    _callback = callback;
}

class VersionInfoCmp {
    public:
        bool operator()(const VersionInfo &v1, const VersionInfo &v2) const {
            if (v1.fullVersion() == v2.fullVersion()) {
                // Same build version, use the channel to define priority
                return v1.channel < v2.channel;
            }
            return CommonUtility::isVersionLower(v2.fullVersion(), v1.fullVersion());
        }
};

const VersionInfo &UpdateChecker::versionInfo(const DistributionChannel choosedChannel) {
    const VersionInfo &prodVersion = prodVersionInfo();

    // If the user wants only `Production` versions, just return the current `Production` version.
    if (choosedChannel == DistributionChannel::Prod) return prodVersion;

    // Otherwise, we need to check if there is not a newer version in other channels.
    const VersionInfo &betaVersion =
            _versionsInfo.contains(DistributionChannel::Beta) ? _versionsInfo[DistributionChannel::Beta] : _defaultVersionInfo;
    const VersionInfo &internalVersion = _versionsInfo.contains(DistributionChannel::Internal)
                                                 ? _versionsInfo[DistributionChannel::Internal]
                                                 : _defaultVersionInfo;
    std::set<std::reference_wrapper<const VersionInfo>, VersionInfoCmp> sortedVersionList;
    sortedVersionList.insert(prodVersion);
    sortedVersionList.insert(betaVersion);
    sortedVersionList.insert(internalVersion);
    for (const auto &versionInfo: sortedVersionList) {
        if (versionInfo.get().channel <= choosedChannel) return versionInfo;
    }

    return _defaultVersionInfo;
}

void UpdateChecker::versionInfoReceived(UniqueId jobId) {
    _isVersionReceived = false;
    _versionsInfo.clear();
    LOG_INFO(Log::instance()->getLogger(), "App version info received");

    auto job = JobManager::instance()->getJob(jobId);
    const auto getAppVersionJobPtr = std::dynamic_pointer_cast<GetAppVersionJob>(job);
    if (!getAppVersionJobPtr) {
        LOG_ERROR(Log::instance()->getLogger(), "Could not cast job pointer.");
        _callback();
        return;
    }

    std::string errorCode;
    std::string errorDescr;
    if (getAppVersionJobPtr->hasErrorApi(&errorCode, &errorDescr)) {
        std::stringstream ss;
        ss << errorCode.c_str() << " - " << errorDescr;
        sentry::Handler::captureMessage(sentry::Level::Warning, "AbstractUpdater::checkUpdateAvailable", ss.str());
        LOG_ERROR(Log::instance()->getLogger(), ss.str().c_str());
    } else if (getAppVersionJobPtr->exitCode() != ExitCode::Ok) {
        LOG_ERROR(Log::instance()->getLogger(), "Error in UpdateChecker::versionInfoReceived : exit code: "
                                                        << getAppVersionJobPtr->exitCode()
                                                        << ", exit cause: " << getAppVersionJobPtr->exitCause());
    } else {
        _versionsInfo = getAppVersionJobPtr->versionsInfo();
        _prodVersionChannel = getAppVersionJobPtr->prodVersionChannel();
        _isVersionReceived = true;
    }

    _callback();
}

ExitCode UpdateChecker::generateGetAppVersionJob(std::shared_ptr<AbstractNetworkJob> &job) {
    AppStateValue appStateValue = "";
    if (bool found = false; !ParmsDb::instance()->selectAppState(AppStateKey::AppUid, appStateValue, found) || !found) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAppState");
        return ExitCode::DbError;
    }

    std::vector<User> userList;
    if (!ParmsDb::instance()->selectAllUsers(userList)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllUsers");
        return ExitCode::DbError;
    }
    std::vector<int> userIdList;
    for (const auto &user: userList) {
        userIdList.push_back(user.userId());
    }

    const auto &appUid = std::get<std::string>(appStateValue);
    job = std::make_shared<GetAppVersionJob>(CommonUtility::platform(), appUid, userIdList);
    return ExitCode::Ok;
}

} // namespace KDC
