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
    CommonUtility::readValueFromStruct(dstruct, "name", name);
    CommonUtility::readValueFromStruct(dstruct, "nodeId", nodeId);
}

ExitInfo NodeCreateMissingFoldersJob::deserializeInputParms() {
    constexpr auto logMessage = "Exception in NodeCreateMissingFoldersJob::readParamValue: error=";
    try {
        readParamValue(inParamsDriveDbId, _driveDbId);
        readParamValues(inParamsFolderList, _folderList, dynamicVar2Struct<FolderItem>);
    } catch (const Poco::Exception &pocoException) {
        LOG_WARN(_logger, logMessage << pocoException.message());

        return ExitCode::LogicError;
    } catch (const CommonUtility::InvalidEnumerationValue &cuException) {
        LOG_WARN(_logger, logMessage << cuException.what());

        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo NodeCreateMissingFoldersJob::serializeOutputParms() {
    writeParamValue(outParamsParentNodeId, _parentNodeId);

    return ExitCode::Ok;
}

ExitInfo NodeCreateMissingFoldersJob::getMissingFoldersInfo(const FolderItem &folderItem, MissingFoldersInfo &info) {
    if (!folderItem.nodeId.empty()) {
        info.parentNodeId = folderItem.nodeId;

        return ExitCode::Ok;
    }

    if (const auto exitCode = ServerRequests::createDir(_driveDbId, info.parentNodeId, folderItem.name, info.parentNodeId);
        exitCode != ExitCode::Ok) {
        LOG_WARN(_logger, "Error in Requests::createDir for driveDbId=" << _driveDbId << " parentNodeId=" << info.parentNodeId);
        addError(Error(ERR_ID, exitCode));

        return exitCode;
    }

    if (info.firstCreatedNodeId.empty()) info.firstCreatedNodeId = info.parentNodeId;

    return ExitCode::Ok;
}

ExitInfo NodeCreateMissingFoldersJob::process() {
    // Pause all syncs of the drive
    std::vector<int> pausedSyncs;

    const std::scoped_lock lock(_commManager->appServer().syncPalMapMutex);
    for (const auto &[syncPalId, syncPal]: _commManager->appServer().syncPalMap) {
        if (!syncPal || syncPal->driveDbId() != _driveDbId || syncPal->isPaused()) continue;
        syncPal->pause();
        pausedSyncs.push_back(syncPalId);
    }

    // Create missing folders
    MissingFoldersInfo foldersInfo;
    foldersInfo.parentNodeId = NodeId(SyncDb::driveRootNode().nodeIdRemote().value());
    for (const auto &folderItem: _folderList) {
        if (const auto exitInfo = getMissingFoldersInfo(folderItem, foldersInfo); !exitInfo) return exitInfo;
    }

    // Add the first created node to the blacklist of every sync
    for (const auto &[syncPalId, syncPal]: _commManager->appServer().syncPalMap) {
        if (!syncPal || syncPal->driveDbId() != _driveDbId) continue;

        // Get blacklist
        NodeSet nodeIdSet;
        if (const auto exitCode = syncPal->syncIdSet(SyncNodeType::BlackList, nodeIdSet); exitCode != ExitCode::Ok) {
            LOG_WARN(_logger, "Error in SyncPal::syncIdSet for syncDbId=" << syncPalId);
            addError(Error(ERR_ID, exitCode));

            return exitCode;
        }

        // Insert the new folder node and set blacklist
        (void) nodeIdSet.insert(foldersInfo.firstCreatedNodeId);
        if (const auto exitCode = syncPal->setSyncIdSet(SyncNodeType::BlackList, nodeIdSet); exitCode != ExitCode::Ok) {
            LOG_WARN(_logger, "Error in SyncPal::setSyncIdSet for syncDbId=" << syncPalId);
            addError(Error(ERR_ID, exitCode));

            return exitCode;
        }
    }

    // Resume all paused syncs
    for (const auto syncDbId: pausedSyncs) {
        if (_commManager->appServer().syncPalMap.contains(syncDbId)) _commManager->appServer().syncPalMap[syncDbId]->unpause();
    }

    return ExitCode::Ok;
}

} // namespace KDC
