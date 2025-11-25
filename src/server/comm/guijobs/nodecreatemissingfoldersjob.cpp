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

#include "nodecreatemissingfoldersjob.h"

#include "appserver.h"
#include "requests/serverrequests.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"

// Input parameters keys
static const auto inParamsDriveDbId = "driveDbId";
static const auto inParamsFolderList = "folderList";

// Output parameters keys
static const auto outParamsParentNodeId = "parentNodeId";

namespace KDC {

NodeCreateMissingFoldersJob::NodeCreateMissingFoldersJob(std::shared_ptr<CommManager> commManager, int requestId,
                                                         const Poco::DynamicStruct &inParams,
                                                         std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::NODE_CREATEMISSINGFOLDERS;
}

void NodeCreateMissingFoldersJob::FolderItem::fromDynamicStruct(const Poco::DynamicStruct &dstruct) {
    CommonUtility::readValueFromStruct(dstruct, "path", path);
    CommonUtility::readValueFromStruct(dstruct, "parentNodeId", parentNodeId);
}

ExitInfo NodeCreateMissingFoldersJob::deserializeInputParms() {
    try {
        readParamValue(inParamsDriveDbId, _driveDbId);
        readParamValues(inParamsFolderList, _folderList, dynamicVar2Struct<FolderItem>);
    } catch (const std::exception &e) {
        LOG_WARN(_logger, "Exception in NodeCreateMissingFoldersJob::readParamValue: error=" << e.what());
        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo NodeCreateMissingFoldersJob::serializeOutputParms() {
    writeParamValue(outParamsParentNodeId, _parentNodeId);

    return ExitCode::Ok;
}

ExitInfo NodeCreateMissingFoldersJob::process() {
    // Pause all syncs of the drive
    std::vector<int> pausedSyncs;

    const std::scoped_lock lock(AppServer::syncPalMapMutex);
    for (const auto &[syncPalId, syncPal]: AppServer::syncPalMap) {
        if (!syncPal) continue;
        if (syncPal->driveDbId() == _driveDbId && !syncPal->isPaused()) {
            syncPal->pause();
            pausedSyncs.push_back(syncPalId);
        }
    }

    // Create missing folders
    NodeId parentNodeId(SyncDb::driveRootNode().nodeIdRemote().value());
    NodeId firstCreatedNodeId;
    for (auto &folderItem: _folderList) {
        if (folderItem.parentNodeId.empty()) {
            if (const auto exitCode = ServerRequests::createDir(_driveDbId, parentNodeId, folderItem.path, parentNodeId);
                exitCode != ExitCode::Ok) {
                LOG_WARN(_logger,
                         "Error in Requests::createDir for driveDbId=" << _driveDbId << " parentNodeId=" << parentNodeId);
                AppServer::addError(Error(ERR_ID, exitCode));
                return exitCode;
            }
            if (firstCreatedNodeId.empty()) {
                firstCreatedNodeId = parentNodeId;
            }
        } else {
            parentNodeId = folderItem.parentNodeId;
        }
    }

    // Add first created node to blacklist of all syncs
    for (const auto &[syncPalId, syncPal]: AppServer::syncPalMap) {
        if (!syncPal) continue;
        if (syncPal->driveDbId() == _driveDbId) {
            // Get blacklist
            NodeSet nodeIdSet;

            if (const auto exitCode = syncPal->syncIdSet(SyncNodeType::BlackList, nodeIdSet); exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in SyncPal::syncIdSet for syncDbId=" << syncPalId);
                AppServer::addError(Error(ERR_ID, exitCode));

                return exitCode;
            }

            // Set blacklist
            nodeIdSet.insert(firstCreatedNodeId);
            if (const auto exitCode = syncPal->setSyncIdSet(SyncNodeType::BlackList, nodeIdSet); exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in SyncPal::setSyncIdSet for syncDbId=" << syncPalId);
                AppServer::addError(Error(ERR_ID, exitCode));

                return exitCode;
            }
        }
    }

    // Resume all paused syncs
    for (int syncDbId: pausedSyncs) {
        if (AppServer::syncPalMap.contains(syncDbId)) {
            AppServer::syncPalMap[syncDbId]->unpause();
        }
    }

    return ExitCode::Ok;
}

} // namespace KDC
