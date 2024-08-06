
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
#include "libcommon/utility/utility.h"
#include "log/log.h"

namespace KDC {

AbstractUpdater* AbstractUpdater::_instance = nullptr;

AbstractUpdater* AbstractUpdater::instance() {
    if (!_instance) {
        _instance = new AbstractUpdater();
    }
    return _instance;
}

ExitCode AbstractUpdater::checkUpdateAvailable(bool& available) {
    AppStateValue appStateValue = LogUploadState::None;
    if (bool found = false; !ParmsDb::instance()->selectAppState(AppStateKey::AppUid, appStateValue, found) || !found) {
        LOG_ERROR(_logger, "Error in ParmsDb::selectAppState");
        return ExitCodeDbError;
    }

    const auto& appUid = std::get<std::string>(appStateValue);
    GetAppVersionJob job(CommonUtility::platformName().toStdString(), appUid);
    job.runSynchronously();
    available = false;
    return ExitCodeOk;
}

AbstractUpdater::AbstractUpdater() {
    _thread = std::make_unique<std::thread>(run);
}

void AbstractUpdater::run() noexcept {
    // To be implemented
}

}  // namespace KDC