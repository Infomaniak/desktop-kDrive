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
#include "../guijobmanager.h"
#include "appserver.h"
#include "requests/serverrequests.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"

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
    } catch (std::exception &e) {
        LOG_WARN(_logger, "Exception in AbstractGuiJob::readParamValue: error=" << e.what());
        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo DriveSearchJob::serializeOutputParms() {
    // Output parameters serialization
    std::function<Poco::Dynamic::Var(const SearchInfo &)> searchInfo2DynamicVar = [](const SearchInfo &value) {
        Poco::DynamicStruct structValue;
        value.toDynamicStruct(structValue);
        return structValue;
    };
    writeParamValues(outParamsSearchInfoList, _searchInfoList, searchInfo2DynamicVar);
    writeParamValue(outParamsHasMore, _hasMore);

    return ExitCode::Ok;
}

ExitInfo DriveSearchJob::process() {
    // Get syncs to delete
    std::vector<int> syncDbIdList;
    for (const auto &syncPalMapElt: _commManager->appServer().syncPalMap()) {
        if (!syncPalMapElt.second) continue;
        if (syncPalMapElt.second->driveDbId() == _driveDbId) {
            syncDbIdList.push_back(syncPalMapElt.first);
        }
    }

    // Stop syncs for this user and remove them from syncPalMap.
    _commManager->appServer().stopAllSyncsTask(syncDbIdList);
    _commManager->appServer().deleteDrive(_driveDbId);
#if defined(KD_MACOS)
    Utility::restartFinderExtension();
#endif

    return ExitCode::Ok;
}

} // namespace KDC
