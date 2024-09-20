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

#include <memory>

#include "db/parmsdb.h"
#include "jobs/network/getappversionjob.h"
#include "jobs/network/API_v2/downloadjob.h"
#include "libcommon/utility/utility.h"
#include "log/log.h"
#include "sparkleupdater.h"
#include "jobs/jobmanager.h"
#include "utility/utility.h"

namespace KDC {

constexpr size_t oneHour = 3600000;

UpdateManager *UpdateManager::_instance = nullptr;
AbstractUpdater *UpdateManager::_updater = nullptr;

UpdateManager *UpdateManager::instance() {
    if (!_instance) _instance = new UpdateManager();
    return _instance;
}

std::string UpdateManager::statusString() const {
    // TODO : To be implemented
    return "";
}

bool UpdateManager::isUpdateDownloaded() {
    // TODO : To be implemented
    return true;
}

UpdateManager::UpdateManager() {
    createUpdater();
}

ExitCode UpdateManager::checkUpdateAvailable(UniqueId *id /*= nullptr*/) {
    _state = UpdateStateV2::Checking;
    std::shared_ptr<AbstractNetworkJob> job;
    if (const auto exitCode = generateGetAppVersionJob(job); exitCode != ExitCode::Ok) {
        return exitCode;
    }
    if (id) *id = job->jobId();

    const std::function<void(UniqueId)> callback = std::bind_front(&UpdateManager::versionInfoReceived, this);
    JobManager::instance()->queueAsyncJob(job, Poco::Thread::PRIO_NORMAL, callback);
    return ExitCode::Ok;
}

void UpdateManager::onUpdateFound() const {
    _updater->onUpdateFound(_versionInfo.downloadUrl);
}

ExitCode UpdateManager::downloadUpdate() noexcept {
    // if (std::error_code ec; std::filesystem::exists(_targetFile, ec)) {
    //     return ExitCode::Ok;
    // }
    //
    // const SyncPath targetPath(CommonUtility::getAppSupportDir());
    // const std::string::size_type pos = _versionInfo.downloadUrl.find_last_of('/');
    // _targetFile = targetPath / _versionInfo.downloadUrl.substr(pos + 1);
    //
    // // TODO : start a simple download job


    return ExitCode::Ok;
}

ExitCode UpdateManager::generateGetAppVersionJob(std::shared_ptr<AbstractNetworkJob> &job) {
    AppStateValue appStateValue = "";
    if (bool found = false; !ParmsDb::instance()->selectAppState(AppStateKey::AppUid, appStateValue, found) || !found) {
        LOG_ERROR(Log::instance()->getLogger(), "Error in ParmsDb::selectAppState");
        setState(UpdateStateV2::Error);
        return ExitCode::DbError;
    }

    const auto &appUid = std::get<std::string>(appStateValue);
    job = std::make_shared<GetAppVersionJob>(CommonUtility::platform(), appUid);
    return ExitCode::Ok;
}

void UpdateManager::versionInfoReceived(UniqueId jobId) {
    auto job = JobManager::instance()->getJob(jobId);
    const auto getAppVersionJobPtr = std::dynamic_pointer_cast<GetAppVersionJob>(job);
    assert(getAppVersionJobPtr);

    std::string errorCode;
    std::string errorDescr;
    if (getAppVersionJobPtr->hasErrorApi(&errorCode, &errorDescr)) {
        std::stringstream ss;
        ss << errorCode.c_str() << " - " << errorDescr;
        SentryHandler::instance()->captureMessage(SentryLevel::Warning, "AbstractUpdater::checkUpdateAvailable", ss.str());
        LOG_ERROR(Log::instance()->getLogger(), ss.str().c_str());
        setState(UpdateStateV2::Error);
        return;
    }

    // TODO : Support all update types
    _versionInfo = getAppVersionJobPtr->getVersionInfo(DistributionChannel::Internal);
    if (!_versionInfo.isValid()) {
        std::string error = "Invalid version info!";
        SentryHandler::instance()->captureMessage(SentryLevel::Warning, "AbstractUpdater::checkUpdateAvailable", error);
        LOG_ERROR(Log::instance()->getLogger(), error.c_str());
        setState(UpdateStateV2::Error);
        return;
    }

    bool available = CommonUtility::isVersionLower(CommonUtility::currentVersion(), _versionInfo.fullVersion());
    _state = available ? UpdateStateV2::Available : UpdateStateV2::UpToDate;
}

void UpdateManager::createUpdater() {
#if defined(Q_OS_MAC) && defined(HAVE_SPARKLE)
    _updater = new SparkleUpdater();
#elif defined(Q_OS_WIN32)
    // Also for MSI
    _instance = new NSISUpdater(url);
#else
    // the best we can do is notify about updates
    _instance = new PassiveUpdateNotifier(url);
#endif
}

void UpdateManager::setState(const UpdateStateV2 newState) {
    _state = newState;
    LOG_DEBUG(Log::instance()->getLogger(), "Update state changed to: " << newState);
    if (_stateChangeCallback) _stateChangeCallback(_state);
}

} // namespace KDC
