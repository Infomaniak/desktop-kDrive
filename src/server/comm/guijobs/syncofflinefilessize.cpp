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

#include "syncofflinefilessize.h"
#include "appserver.h"
#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"
#include <requests/offlinefilessizeestimator.h>
#include <mutex>

// Input parameters keys
static const auto inParamsSyncDbId = "syncDbId";

// Output parameters keys
static const auto outParamsSize = "size";

namespace KDC {

SyncOfflineFileSize::SyncOfflineFileSize(std::shared_ptr<CommManager> commManager, int requestId,
                                         const Poco::DynamicStruct &inParams, std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::SYNC_OFFLINE_FILES_SIZE;
}

ExitInfo SyncOfflineFileSize::deserializeInputParms() {
    try {
        readParamValue(inParamsSyncDbId, _syncDbId);
    } catch (const std::exception &e) {
        LOG_WARN(_logger, "Exception in SyncOfflineFileSize::readParamValue: error=" << e.what());
        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo SyncOfflineFileSize::serializeOutputParms() {
    writeParamValue(outParamsSize, _size);
    return ExitCode::Ok;
}

ExitInfo SyncOfflineFileSize::process() {
    std::vector<std::shared_ptr<SyncPal>> syncPals;

    {
        auto lock = std::scoped_lock(_commManager->appServer().syncPalMapMutex);

        auto it = std::find_if(_commManager->appServer().syncPalMap.begin(), _commManager->appServer().syncPalMap.end(),
                               [this](auto const &pair) {
                                   const auto &syncpal = pair.second;
                                   return syncpal && syncpal->syncDbId() == _syncDbId && syncpal->vfs();
                               });

        if (it != _commManager->appServer().syncPalMap.end()) {
            syncPals.push_back(it->second);
        }
    }

    // Estimate offline file sizes
    OfflineFilesSizeEstimator estimator(syncPals);

    if (const ExitInfo exitInfo = estimator.runSynchronously(); !exitInfo) {
        LOG_WARN(_logger,
                 "Failed to estimate offline files size for syncDbId=" << _syncDbId << " - exitInfo=" << exitInfo);
        return exitInfo;
    }

    _size = estimator.offlineFilesTotalSize();
    return ExitCode::Ok;
}


} // namespace KDC
