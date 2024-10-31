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

#include "updatechecker.h"

#include "db/parmsdb.h"
#include "jobs/jobmanager.h"
#include "jobs/network/abstractnetworkjob.h"
#include "jobs/network/getappversionjob.h"
#include "libcommon/utility/utility.h"
#include "log/log.h"

namespace KDC {

ExitCode UpdateChecker::checkUpdateAvailability(const DistributionChannel channel, UniqueId *id /*= nullptr*/) {
    _channel = channel;

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

void UpdateChecker::versionInfoReceived(UniqueId jobId) {
    _versionInfo.clear();
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
        SentryHandler::instance()->captureMessage(SentryLevel::Warning, "AbstractUpdater::checkUpdateAvailable", ss.str());
        LOG_ERROR(Log::instance()->getLogger(), ss.str().c_str());
    } else {
        DistributionChannel channel = _channel;
        // hasProdNext() is true if and only if the running application has been elected for an upgrade in the context of a
        // progressive update
        if (channel == DistributionChannel::Prod && getAppVersionJobPtr->hasProdNext()) channel = DistributionChannel::Next;

        _versionInfo = getAppVersionJobPtr->getVersionInfo(channel);
        if (!_versionInfo.isValid()) {
            std::string error = "Invalid version info!";
            SentryHandler::instance()->captureMessage(SentryLevel::Warning, "AbstractUpdater::checkUpdateAvailable", error);
            LOG_ERROR(Log::instance()->getLogger(), error.c_str());
        }
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
