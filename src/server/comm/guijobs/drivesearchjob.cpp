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

#include "drivesearchjob.h"
#include "appserver.h"
#include "requests/serverrequests.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"
#include "libsyncengine/jobs/network/kDrive_API/searchjob.h"

// Input parameters keys
static const auto inParamsDriveDbId = "driveDbId";
static const auto inParamsSearchString = "searchString";

// Output parameters keys
static const auto outParamsSearchInfoList = "searchInfoList";
static const auto outParamsHasMore = "hasMore";

namespace KDC {

DriveSearchJob::DriveSearchJob(std::shared_ptr<CommManager> commManager, int requestId, const Poco::DynamicStruct &inParams,
                               std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::DRIVE_SEARCH;
}

ExitInfo DriveSearchJob::deserializeInputParms() {
    try {
        readParamValue(inParamsDriveDbId, _driveDbId);
        readParamValue(inParamsSearchString, _searchString);
    } catch (const std::exception &e) {
        LOG_WARN(_logger, "Exception in DriveSearchJob::readParamValue: error=" << e.what());
        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo DriveSearchJob::serializeOutputParms() {
    writeParamValues(outParamsSearchInfoList, _searchInfoList, info2DynamicVar<SearchInfo>);
    writeParamValue(outParamsHasMore, _hasMore);

    return ExitCode::Ok;
}

ExitInfo DriveSearchJob::process() {
    // Find drive ID
    Drive drive;
    bool found = false;
    if (!ParmsDb::instance()->selectDrive(_driveDbId, drive, found)) {
        LOG_WARN(_logger, "Error in ParmsDb::selectDrive");
        return ExitCode::DbError;
    }
    if (!found) {
        LOG_WARN(_logger, "Drive not found for ID: " << _driveDbId);
        return ExitCode::DataError;
    }

    // Send search request (synchronously for now)
    SearchJob searchJob(_driveDbId, CommonUtility::commString2Str(_searchString));
    (void) searchJob.runSynchronously();
    for (const auto &searchInfo: searchJob.searchResults()) {
        _searchInfoList.push_back(searchInfo);
    }

    return ExitCode::Ok;
}

} // namespace KDC
