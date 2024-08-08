
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

#include <version.h>

#include "db/parmsdb.h"
#include "jobs/network/getappversionjob.h"
#include "libcommon/utility/utility.h"
#include "log/log.h"

namespace KDC {
AbstractUpdater *AbstractUpdater::_instance = nullptr;

AbstractUpdater *AbstractUpdater::instance() {
    if (!_instance) {
        _instance = new AbstractUpdater();
    }
    return _instance;
}

ExitCode AbstractUpdater::checkUpdateAvailable(bool &available) {
    AppStateValue appStateValue = "";
    if (bool found = false; !ParmsDb::instance()->selectAppState(AppStateKey::AppUid, appStateValue, found) || !found) {
        LOG_ERROR(_logger, "Error in ParmsDb::selectAppState");
        return ExitCodeDbError;
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
#ifdef NDEBUG
        sentry_capture_event(
            sentry_value_new_message_event(SENTRY_LEVEL_WARNING, "AbstractUpdater::checkUpdateAvailable", ss.str().c_str()));
#endif
        LOG_ERROR(_logger, ss.str().c_str());
        return ExitCodeUpdateFailed;
    }
    // TODO : for now we support only Prod updates
    VersionInfo versionInfo = _getAppVersionJob->getVersionInfo(DistributionChannel::Prod);
    if (!versionInfo.isValid()) {
        std::string error = "Invalid version info!";
#ifdef NDEBUG
        sentry_capture_event(
            sentry_value_new_message_event(SENTRY_LEVEL_WARNING, "AbstractUpdater::checkUpdateAvailable", error.c_str()));
#endif
        LOG_ERROR(_logger, error.c_str());
        return ExitCodeUpdateFailed;
    }

    available = CommonUtility::isVersionLower(currentVersion(), versionInfo.fullVersion());
    return ExitCodeOk;
}

AbstractUpdater::AbstractUpdater() {
    _thread = std::make_unique<std::thread>(run);
}

AbstractUpdater::~AbstractUpdater() {
    if (_getAppVersionJob) {
        delete _getAppVersionJob;
        _getAppVersionJob = nullptr;
    }
}

void AbstractUpdater::run() noexcept {
    // To be implemented
}

std::string AbstractUpdater::currentVersion() {
    static std::string currentVersion =
        std::format("{}.{}.{}.{}", KDRIVE_VERSION_MAJOR, KDRIVE_VERSION_MINOR, KDRIVE_VERSION_PATCH, KDRIVE_VERSION_BUILD);
    return currentVersion;
}
}  // namespace KDC
