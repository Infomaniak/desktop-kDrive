/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#include "nodeconflictinfojob.h"
#include "appserver.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/io/filestat.h"
#include "libsyncengine/jobs/network/kDrive_API/getfileinfojobV3.h"
#include "libsyncengine/jobs/network/kDrive_API/getdriveuserinfojob.h"
#include "libsyncengine/requests/driveuserinfocache.h"
#include "libsyncengine/syncpal/syncpal.h"
#include "libparms/db/parmsdb.h"

// Input parameters keys
static const auto inParamsSyncDbId = "syncDbId";
static const auto inParamsRelativePath = "relativePath";
static const auto inParamsReplicaSide = "replicaSide";

// Output parameters keys
static const auto outParamsNodeConflictInfo = "nodeConflictInfo";

namespace KDC {

NodeConflictInfoJob::NodeConflictInfoJob(std::shared_ptr<CommManager> commManager, int32_t requestId,
                                         const Poco::DynamicStruct &inParams, std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::NODE_CONFLICT_INFO;
}

ExitInfo NodeConflictInfoJob::deserializeInputParms() {
    try {
        readParamValue(inParamsSyncDbId, _syncDbId);

        CommString relativePathStr;
        readParamValue(inParamsRelativePath, relativePathStr);
        _relativePath = SyncPath(relativePathStr);

        readParamValue(inParamsReplicaSide, _replicaSide);
    } catch (const std::exception &e) {
        LOG_WARN(_logger, "Exception in NodeConflictInfoJob::readParamValue: error=" << e.what());
        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo NodeConflictInfoJob::serializeOutputParms() {
    writeParamValue(outParamsNodeConflictInfo, _nodeConflictInfo, info2DynamicVar<NodeConflictInfo>);

    return ExitCode::Ok;
}

ExitInfo NodeConflictInfoJob::fetchRemoteInfo(const UserDbId userDbId, const DriveId driveId, const NodeId &nodeId) {
    std::shared_ptr<GetFileInfoJobV3> networkJob;
    try {
        networkJob = std::make_shared<GetFileInfoJobV3>(userDbId, driveId, nodeId);
    } catch (const std::exception &e) {
        LOG_WARN(_logger, "Error creating GetFileInfoJobV3: error=" << e.what());
        return ExitCode::DataError;
    }

    if (const auto exitInfo = networkJob->runSynchronously(); !exitInfo) {
        LOG_WARN(_logger, "Error in GetFileInfoJobV3::runSynchronously for nodeId=" << nodeId << " exitInfo=" << exitInfo);
        return exitInfo;
    }
    _nodeConflictInfo.setFileSize(networkJob->size());
    _nodeConflictInfo.setLastModificationDate(networkJob->modificationTime());

    // Resolve the author display name from the user ID
    const int32_t lastModifiedByUserId = networkJob->lastModifiedByUserId();
    if (lastModifiedByUserId > 0) {
        // Check cache first
        if (auto cached = DriveUserInfoCache::instance().get(driveId, lastModifiedByUserId)) {
            _nodeConflictInfo.setAuthorName(cached->name);
        } else {
            std::shared_ptr<GetDriveUserInfoJob> userInfoJob;
            try {
                userInfoJob = std::make_shared<GetDriveUserInfoJob>(userDbId, driveId, lastModifiedByUserId);
            } catch (const std::exception &e) {
                LOG_WARN(_logger, "Error creating GetDriveUserInfoJob: error=" << e.what());
                return ExitCode::DataError;
            }

            if (const auto exitInfo = userInfoJob->runSynchronously(); !exitInfo) {
                LOG_WARN(_logger, "Error in GetDriveUserInfoJob::runSynchronously for userId=" << lastModifiedByUserId
                                                                                               << " exitInfo=" << exitInfo);
                return exitInfo;
            }
            _nodeConflictInfo.setAuthorName(userInfoJob->name());
            DriveUserInfoCache::instance().put(driveId, lastModifiedByUserId,
                                               {userInfoJob->name(), userInfoJob->email(), userInfoJob->avatarUrl()});
        }
    }

    return ExitCode::Ok;
}

ExitInfo NodeConflictInfoJob::fetchLocalInfo(const SyncPath &localPath, const UserDbId userDbId) {
    // Get file size and modification date from filesystem
    FileStat fileStat;
    IoError ioError = IoError::Success;
    if (!IoHelper::getFileStat(localPath, &fileStat, ioError, IoHelper::PathCheckOption::Insensitive)) {
        LOGW_WARN(_logger, L"Error in IoHelper::getFileStat for " << Utility::formatIoError(localPath, ioError));
        return ExitCode::SystemError;
    }

    if (ioError != IoError::Success) {
        LOGW_WARN(_logger, L"IoError in IoHelper::getFileStat for " << Utility::formatIoError(localPath, ioError));
        return ExitCode::SystemError;
    }

    _nodeConflictInfo.setFileSize(fileStat.size);
    _nodeConflictInfo.setLastModificationDate(fileStat.modificationTime);

    User user;
    bool found = false;
    if (!ParmsDb::instance()->selectUser(userDbId, user, found)) {
        return ExitCode::DbError;
    } else if (!found) {
        return {ExitCode::DataError, ExitCause::DbEntryNotFound};
    } else {
        _nodeConflictInfo.setAuthorName(user.name());
        return ExitCode::Ok;
    }
}

ExitInfo NodeConflictInfoJob::process() {
    // Look up SyncPal from syncPalMap
    std::shared_ptr<SyncPal> syncPal;
    if (ExitInfo exitInfo = getSyncPal(_syncDbId, syncPal); !exitInfo) {
        return exitInfo;
    }

    const UserDbId userDbId = syncPal->userDbId();
    const DriveId driveId = syncPal->driveId();
    const SyncPath &localRootPath = syncPal->localPath();

    // Resolve the remote nodeId from the relative path using the sync database
    NodeId remoteNodeId;
    if (const auto syncDb = syncPal->syncDb(); syncDb) {
        std::optional<NodeId> nodeIdOpt;
        bool found = false;
        if (syncDb->id(ReplicaSide::Remote, _relativePath, nodeIdOpt, found) && found && nodeIdOpt.has_value()) {
            remoteNodeId = nodeIdOpt.value();
        }
    }

    if (_replicaSide == ReplicaSide::Remote) {
        if (remoteNodeId.empty()) {
            LOGW_WARN(_logger,
                      L"Remote nodeId not found for " << Utility::formatSyncPath(_relativePath) << L" in syncDbId=" << _syncDbId);
            return {ExitCode::DataError, ExitCause::NotFound};
        }

        if (ExitInfo exitInfo = fetchRemoteInfo(userDbId, driveId, remoteNodeId); !exitInfo) {
            LOGW_WARN(_logger, L"Error fetching remote info for nodeId=" << CommonUtility::s2ws(remoteNodeId) << L" exitInfo="
                                                                         << exitInfo);
            return exitInfo;
        }
    } else if (_replicaSide == ReplicaSide::Local) {
        const SyncPath localFilePath = localRootPath / _relativePath;

        if (ExitInfo exitInfo = fetchLocalInfo(localFilePath, userDbId); !exitInfo) {
            LOGW_WARN(_logger,
                      L"Error fetching local info for " << Utility::formatSyncPath(localFilePath) << L" exitInfo=" << exitInfo);
            return exitInfo;
        }
    } else {
        LOG_WARN(_logger, "Invalid ReplicaSide value for syncDbId=" << _syncDbId);
        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

} // namespace KDC
