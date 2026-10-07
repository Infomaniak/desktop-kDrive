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

#include "errorquickresolvehardlinkjob.h"

#include "libcommon/comm.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/utility/utility.h"
#include "libsyncengine/db/dbnode.h"
#include "libsyncengine/jobs/local/genericlocaldeletejob.h"
#include "libsyncengine/jobs/local/localcopyjob.h"
#include "libsyncengine/jobs/local/localcreatedirjob.h"
#include "libsyncengine/propagation/executor/filerescuer.h"
#include "libsyncengine/syncpal/syncpal.h"
#include "libsyncengine/syncpal/useractionscopedlock.h"
#include "signalerrorremovedjob.h"

// Input parameters keys
static const auto inParamsSyncDbId = "syncDbId";
static const auto inParamsErrorDbId = "errorDbId";
static const auto inParamsNodeId = "nodeId";
static const auto inParamsPath = "path";

namespace KDC {

ErrorQuickResolveHardlinkJob::ErrorQuickResolveHardlinkJob(std::shared_ptr<CommManager> commManager, int32_t requestId,
                                                           const Poco::DynamicStruct &inParams,
                                                           std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::ERROR_QUICK_RESOLVE_HARDLINK;
}

ExitInfo ErrorQuickResolveHardlinkJob::deserializeInputParms() {
    try {
        readParamValue(inParamsSyncDbId, _syncDbId);
        readParamValue(inParamsErrorDbId, _errorDbId);
        readParamValue(inParamsNodeId, _nodeId);

        CommString path;
        readParamValue(inParamsPath, path);
        _relativeLocalPath = CommonUtility::commString2SyncPath(path);
    } catch (const std::exception &e) {
        LOG_WARN(_logger, "Exception in ErrorQuickResolveHardlinkJob::readParamValue: error=" << e.what());
        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo ErrorQuickResolveHardlinkJob::process() {
    std::shared_ptr<SyncPal> syncPal;
    if (ExitInfo exitInfo = getSyncPal(_syncDbId, syncPal); !exitInfo) {
        return exitInfo;
    }

    UserActionScopedLock lock;
    if (syncPal != nullptr && !lock.tryLock(syncPal, std::chrono::milliseconds(userActionLockShortTimeoutMs))) {
        LOG_WARN(_logger, "Could not acquire user action lock for syncDbId="
                                  << _syncDbId << ". Another user action is running. Aborting ErrorQuickResolveHardlinkJob.");
        return ExitCode::OperationCanceled;
    }

    if (ExitInfo exitInfo = quickResolve(syncPal); !exitInfo) {
        return exitInfo;
    }

    // Notify the GUI that the error has been resolved so that it can remove the corresponding error card.
    if (_commManager) {
        _commManager->sendGuiSignal(std::make_shared<SignalErrorRemovedJob>(_errorDbId));
    }

    return ExitCode::Ok;
}

ExitInfo ErrorQuickResolveHardlinkJob::quickResolve(const std::shared_ptr<SyncPal> &syncPal) {
    const auto syncDb = syncPal->syncDb();
    const auto localPath = syncPal->localPath();

    // Fetch the corresponding node in the sync database. Deleting this node removes both replicas, so the next synchronization
    // will see the remote file as a new item and will download it again as a standard file.
    DbNode dbNode;
    bool nodeFound = false;
    if (!syncDb->node(ReplicaSide::Local, _nodeId, dbNode, nodeFound)) {
        LOGW_WARN(_logger, L"Error in SyncDb::node for node id " << CommonUtility::s2ws(_nodeId));
        return ExitCode::DbError;
    }

    // Enumerate all the existing paths of the file, starting from the path reported in the error.
    const SyncPath seedPath = localPath / _relativeLocalPath;
    std::vector<SyncPath> hardlinkPaths;
    IoError ioError = IoError::Success;
    if (!IoHelper::getHardlinkPaths(seedPath, hardlinkPaths, ioError, localPath)) {
        if (ioError == IoError::NoSuchFileOrDirectory) {
            // The file does not exist anymore. Only remove the node from the database so that the file is downloaded again.
            LOGW_WARN(_logger, L"The file does not exist anymore: " << Utility::formatSyncPath(seedPath));
        } else {
            LOGW_WARN(_logger, L"Error in IoHelper::getHardlinkPaths for " << Utility::formatSyncPath(seedPath) << L": "
                                                                           << Utility::formatIoError(ioError));
            return ExitCode::SystemError;
        }
    }

    // Keep only the links located under the sync root.
    std::vector<SyncPath> hardlinkPathsUnderSyncRoot;
    for (const auto &path: hardlinkPaths) {
        if (CommonUtility::isDescendantOrEqual(path, localPath)) {
            (void) hardlinkPathsUnderSyncRoot.push_back(path);
        } else {
            LOGW_DEBUG(_logger, L"Link located outside of the sync root, skipping: " << Utility::formatSyncPath(path));
        }
    }

    if (!hardlinkPathsUnderSyncRoot.empty()) {
        // Check whether the local file is in sync with the database. This must be done before removing the node from the
        // database, as the check requires the node to be present.
        const bool inSync = syncPal->isLocalItemInSyncWithDb(seedPath);

        if (!inSync) {
            // The file content may differ from the remote version: save a copy into the rescue folder before removing the
            // hardlinks, so that the user does not lose any data.
            const SyncPath rescueFolderPath = localPath / FileRescuer::rescueFolderName();

            bool rescueFolderExists = false;
            IoError ioError = IoError::Unknown;
            (void) IoHelper::checkIfPathExists(rescueFolderPath, rescueFolderExists, ioError,
                                               IoHelper::PathCheckOption::Insensitive);
            if (ioError != IoError::Success) {
                LOGW_WARN(_logger, L"Failed to check rescue directory existence. Error: " << Utility::formatIoError(ioError));
                return ExitCode::SystemError;
            }
            if (!rescueFolderExists) {
                LocalCreateDirJob createRescueFolderJob(rescueFolderPath);
                if (ExitInfo exitInfo = createRescueFolderJob.runSynchronously(); !exitInfo) {
                    LOGW_WARN(_logger, L"Failed to create the rescue folder "
                                               << Utility::formatSyncPath(rescueFolderPath) << L": " << exitInfo);
                    return exitInfo;
                }
            }

            ExitInfo copyExitInfo;
            SyncPath relativeDestinationPath;
            uint16_t counter = 0;
            do {
                const SyncName suffix =
                        Str(" (") + Str2SyncName(std::to_string(counter)) + Str(")"); // TODO : use format when fully moved to c++20
                const SyncName filename = counter == 0 ? seedPath.filename().native()
                                                       : seedPath.stem().native() + suffix + seedPath.extension().native();
                const SyncPath destinationPath = rescueFolderPath / filename;
                LocalCopyJob copyJob(seedPath, destinationPath);
                copyExitInfo = copyJob.runSynchronously();
                relativeDestinationPath = FileRescuer::rescueFolderName() / filename;
                counter++;
            } while (!copyExitInfo && copyExitInfo.cause() == ExitCause::FileExists);
            if (!copyExitInfo) {
                LOGW_WARN(_logger, L"Failed to copy " << Utility::formatSyncPath(seedPath) << L" into the rescue folder: "
                                                      << copyExitInfo);
                return copyExitInfo;
            }

            const Error error(_syncDbId, _nodeId, dbNode.hasRemoteNodeId() ? dbNode.nodeIdRemote().value() : NodeId(),
                              dbNode.type(), _relativeLocalPath, ConflictType::None, InconsistencyType::None,
                              CancelType::FileRescued, relativeDestinationPath);
            syncPal->addError(error);
        }

        // Remove the item from the temporary blacklist, if any, so that the file system observer processes the deletion of the
        // links and the download of the file during the next synchronization.
        syncPal->removeItemFromTmpBlacklist(_nodeId, ReplicaSide::Local);
        if (nodeFound && dbNode.hasRemoteNodeId()) {
            syncPal->removeItemFromTmpBlacklist(dbNode.nodeIdRemote().value(), ReplicaSide::Remote);
        }

        // Hard remove all the links located under the sync root.
        for (const auto &path: hardlinkPathsUnderSyncRoot) {
            GenericLocalDeleteJob deleteJob(path, syncPal->cacheDirectory(), GenericLocalDeleteJob::ForceHardDelete::Yes);
            if (ExitInfo exitInfo = deleteJob.runSynchronously(); !exitInfo) {
                LOGW_WARN(_logger, L"Failed to delete " << Utility::formatSyncPath(path) << L": " << exitInfo);
                return exitInfo;
            }
        }
    }

    // Remove the node from the sync database.
    if (nodeFound) {
        bool deleteNodeFound = false;
        if (!syncDb->deleteNode(dbNode.nodeId(), deleteNodeFound)) {
            LOGW_WARN(_logger, L"Error in SyncDb::deleteNode for DB node ID=" << dbNode.nodeId());
            return ExitCode::DbError;
        }
        if (!deleteNodeFound) {
            LOGW_WARN(_logger, L"Node not found in the sync database for DB node ID=" << dbNode.nodeId());
        }
    }

    LOG_INFO(_logger, "Hardlink quick resolve done for syncDbId=" << _syncDbId << ", errorDbId=" << _errorDbId << ", nodeId="
                                                                  << _nodeId);

    return ExitCode::Ok;
}

} // namespace KDC
