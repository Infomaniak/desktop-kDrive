
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

#include "abstractupdater.h"

#include "db/parmsdb.h"
#include "jobs/network/getappversionjob.h"
#include "jobs/network/API_v2/downloadjob.h"
#include "libcommon/utility/utility.h"
#include "log/log.h"
#include "utility/utility.h"

namespace KDC {

constexpr size_t oneHour = 3600000;

AbstractUpdater *AbstractUpdater::_instance = nullptr;
std::unique_ptr<std::thread> AbstractUpdater::_thread;

AbstractUpdater *AbstractUpdater::instance(bool test /*= false*/) {
    if (!_instance) {
        _instance = new AbstractUpdater();
        if (!test) { // Do not start thread in test mode
            _thread = std::make_unique<std::thread>(&AbstractUpdater::run, _instance);
        }
    }
    return _instance;
}

ExitCode AbstractUpdater::checkUpdateAvailable(bool &available) {
    AppStateValue appStateValue = "";
    if (bool found = false; !ParmsDb::instance()->selectAppState(AppStateKey::AppUid, appStateValue, found) || !found) {
        LOG_ERROR(_logger, "Error in ParmsDb::selectAppState");
        return ExitCode::DbError;
    }

    const auto &appUid = std::get<std::string>(appStateValue);
    if (!_getAppVersionJob) {
        _getAppVersionJob = new GetAppVersionJob(CommonUtility::platform(), appUid);
    }
    _getAppVersionJob->runSynchronously();

    std::string errorCode;
    std::string errorDescr;
    if (_getAppVersionJob->hasErrorApi(&errorCode, &errorDescr)) {
        std::stringstream ss;
        ss << errorCode.c_str() << " - " << errorDescr;
        SentryHandler::instance()->captureMessage(SentryLevel::Warning, "AbstractUpdater::checkUpdateAvailable", ss.str());
        LOG_ERROR(_logger, ss.str().c_str());
        return ExitCode::UpdateFailed;
    }
    // TODO : Support all update types
    _versionInfo = _getAppVersionJob->getVersionInfo(DistributionChannel::Internal);
    if (!_versionInfo.isValid()) {
        std::string error = "Invalid version info!";
        SentryHandler::instance()->captureMessage(SentryLevel::Warning, "AbstractUpdater::checkUpdateAvailable", error);
        LOG_ERROR(_logger, error.c_str());
        return ExitCode::UpdateFailed;
    }

    available = CommonUtility::isVersionLower(CommonUtility::currentVersion(), _versionInfo.fullVersion());
    return ExitCode::Ok;
}

bool AbstractUpdater::isUpdateDownloaded() {
    // To be implemented
    return true;
}

ExitCode AbstractUpdater::downloadUpdate() noexcept {
    std::error_code ec;
    if (std::filesystem::exists(_targetFile, ec)) {
        return ExitCode::Ok;
    }

    const SyncPath targetPath(CommonUtility::getAppSupportDir());
    const std::string::size_type pos = _versionInfo.downloadUrl.find_last_of('/');
    _targetFile = targetPath / _versionInfo.downloadUrl.substr(pos + 1);

    // TODO : start a simple download job

    return ExitCode::Ok;
}

AbstractUpdater::~AbstractUpdater() {
    if (_getAppVersionJob) {
        delete _getAppVersionJob;
        _getAppVersionJob = nullptr;
    }
    _thread->detach();
}

void AbstractUpdater::run() noexcept {
    switch (_state) {
        case UpdateStateV2::UpToDate: {
            bool updateAvailable = false;
            if (const ExitCode exitCode = checkUpdateAvailable(updateAvailable); exitCode != ExitCode::Ok) {
                // TODO : how do we give feedback to main loop about update errors?
                _state = UpdateStateV2::Error;
                break;
            }

            if (updateAvailable) {
                _state = UpdateStateV2::Available;
            } else {
                Utility::msleep(oneHour); // Sleep for 1h
            }
            break;
        }
        case UpdateStateV2::Available: {
            if (isUpdateDownloaded()) {
                _state = UpdateStateV2::Ready;
                break;
            }
            if (const ExitCode exitCode = downloadUpdate(); exitCode != ExitCode::Ok) {
                // TODO : how do we give feedback to main loop about update errors?
                _state = UpdateStateV2::Error;
                break;
            }
            break;
        }
        case UpdateStateV2::Downloading:
        case UpdateStateV2::Ready:
        case UpdateStateV2::Error:
        default:
            break;
    }
}

} // namespace KDC
