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
static const auto inParamsUserDbId = "userDbId";
static const auto inParamsDriveId = "driveId";
static const auto inParamsParentNodeId = "parentNodeId";
static const auto inParamsRelativePath = "relativePath";

// Output parameters keys
static const auto outParamsNodeId = "nodeId";

namespace KDC {

NodeCreateMissingFoldersJob::NodeCreateMissingFoldersJob(std::shared_ptr<CommManager> commManager, int requestId,
                                                         const Poco::DynamicStruct &inParams,
                                                         std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::NODE_CREATEMISSINGFOLDERS;
}


ExitInfo NodeCreateMissingFoldersJob::deserializeInputParms() {
    constexpr auto logMessage = "Exception in NodeCreateMissingFoldersJob::readParamValue: error=";
    try {
        readParamValue(inParamsUserDbId, _userDbId);
        readParamValue(inParamsDriveId, _driveId);
        CommString commStr;
        readParamValue(inParamsRelativePath, commStr);
        _relativePath = SyncPath(commStr);
        readParamValue(inParamsParentNodeId, commStr);
        _parentNodeId = CommonUtility::commString2Str(commStr);
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
    writeParamValue(outParamsNodeId, _parentNodeId);

    return ExitCode::Ok;
}

ExitInfo NodeCreateMissingFoldersJob::pauseDriveSyncs(std::vector<int> &pausedSyncs) {
    const std::scoped_lock lock(_commManager->appServer().syncPalMapMutex);
    for (const auto &[syncPalId, syncPal]: _commManager->appServer().syncPalMap) {
        if (!syncPal || syncPal->driveId() != _driveId || syncPal->isPaused()) continue;
        syncPal->pause();
        pausedSyncs.push_back(syncPalId);
    }
    return ExitCode::Ok;
}

ExitInfo NodeCreateMissingFoldersJob::createMissingFolders(NodeId &firstCreatedNodeId) {
    for (const auto &folderName: _relativePath) {
        if (const auto exitCode = ServerRequests::createDir(_userDbId, _driveId, _parentNodeId, folderName, _parentNodeId);
            exitCode != ExitCode::Ok) {
            LOG_WARN(_logger, "Error in Requests::createDir for driveId=" << _driveId << " parentNodeId=" << _parentNodeId);
            addError(Error(ERR_ID, exitCode));
            return exitCode;
        }
        if (firstCreatedNodeId.empty()) firstCreatedNodeId = _parentNodeId;
    }
    return ExitCode::Ok;
}

ExitInfo NodeCreateMissingFoldersJob::blacklistNodeOnAllDriveSyncs(const NodeId &nodeId) {
    const std::scoped_lock lock(_commManager->appServer().syncPalMapMutex);
    for (const auto &[syncPalId, syncPal]: _commManager->appServer().syncPalMap) {
        if (!syncPal || syncPal->driveId() != _driveId) continue;

        NodeSet nodeIdSet;
        if (const auto exitCode = syncPal->syncIdSet(SyncNodeType::BlackList, nodeIdSet); exitCode != ExitCode::Ok) {
            LOG_WARN(_logger, "Error in SyncPal::syncIdSet for syncDbId=" << syncPalId);
            addError(Error(ERR_ID, exitCode));
            return exitCode;
        }

        (void) nodeIdSet.insert(nodeId);
        if (const auto exitCode = syncPal->setSyncIdSet(SyncNodeType::BlackList, nodeIdSet); exitCode != ExitCode::Ok) {
            LOG_WARN(_logger, "Error in SyncPal::setSyncIdSet for syncDbId=" << syncPalId);
            addError(Error(ERR_ID, exitCode));
            return exitCode;
        }
    }
    return ExitCode::Ok;
}

void NodeCreateMissingFoldersJob::resumeSyncs(const std::vector<int> &pausedSyncs) {
    const std::scoped_lock lock(_commManager->appServer().syncPalMapMutex);
    for (const auto syncDbId: pausedSyncs) {
        if (_commManager->appServer().syncPalMap.contains(syncDbId)) _commManager->appServer().syncPalMap[syncDbId]->unpause();
    }
}

ExitInfo NodeCreateMissingFoldersJob::process() {
    std::vector<int> pausedSyncs;
    if (const auto exitInfo = pauseDriveSyncs(pausedSyncs); !exitInfo) return exitInfo;

    NodeId firstCreatedNodeId;
    if (const auto exitInfo = createMissingFolders(firstCreatedNodeId); !exitInfo) return exitInfo;

    if (const auto exitInfo = blacklistNodeOnAllDriveSyncs(firstCreatedNodeId); !exitInfo) return exitInfo;

    resumeSyncs(pausedSyncs);
    return ExitCode::Ok;
}

} // namespace KDC
