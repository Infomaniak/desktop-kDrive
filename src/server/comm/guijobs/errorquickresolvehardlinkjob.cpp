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
#include "libcommonserver/io/filestat.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/utility/utility.h"
#include "libparms/db/parmsdb.h"
#include "libsyncengine/db/dbnode.h"
#include "libsyncengine/jobs/local/genericlocaldeletejob.h"
#include "libsyncengine/jobs/local/localcopyjob.h"
#include "libsyncengine/jobs/local/localcreatedirjob.h"
#include "libsyncengine/propagation/executor/filerescuer.h"
#include "libsyncengine/syncpal/syncpal.h"
#include "libsyncengine/syncpal/useractionscopedlock.h"
#include "signalerrorremovedjob.h"

#include <algorithm>

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

ExitInfo ErrorQuickResolveHardlinkJob::quickResolve(const std::shared_ptr<SyncPal> &syncPal) const {
    // Reject an outdated or inconsistent request before any change, so that the error removed at the end is the one of the
    // processed node.
    if (ExitInfo exitInfo = checkParmsDbError(); !exitInfo) {
        return exitInfo;
    }

    // Fetch the corresponding node in the sync database. Deleting this node removes both replicas, so the next synchronization
    // will see the remote file as a new item and will download it again as a standard file.
    DbNode dbNode;
    if (ExitInfo exitInfo = fetchFileDbNode(syncPal, dbNode); !exitInfo) {
        return exitInfo;
    }

    SyncPath seedPath;
    if (ExitInfo exitInfo = getSeedPath(syncPal->localPath(), seedPath); !exitInfo) {
        return exitInfo;
    }

    bool seedExists = false;
    if (ExitInfo exitInfo = checkSeedItem(seedPath, seedExists); !exitInfo) {
        return exitInfo;
    }

    // If the file does not exist anymore, only remove the node from the database so that the file is downloaded again.
    if (seedExists) {
        if (ExitInfo exitInfo = removeLinks(syncPal, dbNode, seedPath); !exitInfo) {
            return exitInfo;
        }
    }

    if (ExitInfo exitInfo = deleteDbNode(syncPal, dbNode); !exitInfo) {
        return exitInfo;
    }

    if (ExitInfo exitInfo = deleteParmsDbError(); !exitInfo) {
        return exitInfo;
    }

    LOG_INFO(_logger,
             "Hardlink quick resolve done for syncDbId=" << _syncDbId << ", errorDbId=" << _errorDbId << ", nodeId=" << _nodeId);

    return ExitCode::Ok;
}

ExitInfo ErrorQuickResolveHardlinkJob::checkParmsDbError() const {
    Error error;
    bool errorFound = false;
    if (!ParmsDb::instance()->selectError(_errorDbId, error, errorFound)) {
        LOG_WARN(_logger, "Error in ParmsDb::selectError for errorDbId=" << _errorDbId);
        return ExitCode::DbError;
    }
    if (!errorFound) {
        LOG_WARN(_logger, "Error not found in the parameters database for errorDbId=" << _errorDbId);
        return ExitCode::InvalidOperation;
    }

    if (error.level() != ErrorLevel::Node || error.exitCode() != ExitCode::SystemError ||
        error.exitCause() != ExitCause::HardlinkNotSupported) {
        LOG_WARN(_logger, "The error with errorDbId=" << _errorDbId << " is not a hardlink error: level="
                                                      << toString(error.level()) << ", exitCode=" << toString(error.exitCode())
                                                      << ", exitCause=" << toString(error.exitCause()));
        return ExitCode::InvalidOperation;
    }

    // Paths are compared element by element, so the result does not depend on the directory separator used by the GUI.
    if (error.syncDbId() != _syncDbId || _nodeId.empty() || error.localNodeId() != _nodeId ||
        error.path() != _relativeLocalPath) {
        LOGW_WARN(_logger, L"The error with errorDbId=" << _errorDbId << L" does not match the reported item: syncDbId="
                                                        << error.syncDbId() << L", nodeId="
                                                        << CommonUtility::s2ws(error.localNodeId()) << L", path="
                                                        << Utility::formatSyncPath(error.path()));
        return ExitCode::InvalidOperation;
    }

    return ExitCode::Ok;
}

ExitInfo ErrorQuickResolveHardlinkJob::fetchFileDbNode(const std::shared_ptr<SyncPal> &syncPal, DbNode &dbNode) const {
    bool nodeFound = false;
    if (!syncPal->syncDb()->node(ReplicaSide::Local, _nodeId, dbNode, nodeFound)) {
        LOGW_WARN(_logger, L"Error in SyncDb::node for node id " << CommonUtility::s2ws(_nodeId));
        return ExitCode::DbError;
    }
    if (!nodeFound) {
        // The node is no longer in the sync database: reject the request before any file system operation, as the next
        // synchronization already handles the item.
        LOGW_WARN(_logger, L"Node not found in the sync database for node id " << CommonUtility::s2ws(_nodeId));
        return ExitCode::InvalidOperation;
    }
    if (dbNode.type() != NodeType::File) {
        // Only files can have hardlinks: never remove a directory, as its whole content would be removed.
        LOGW_WARN(_logger, L"The node with node id " << CommonUtility::s2ws(_nodeId) << L" is not a file");
        return ExitCode::InvalidOperation;
    }

    return ExitCode::Ok;
}

ExitInfo ErrorQuickResolveHardlinkJob::getSeedPath(const SyncPath &localPath, SyncPath &seedPath) const {
    // The path is provided by the GUI: reject any empty, absolute or escaping path before any file system operation.
    if (_relativeLocalPath.empty() || _relativeLocalPath.is_absolute()) {
        LOGW_WARN(_logger, L"The reported path is empty or absolute: " << Utility::formatSyncPath(_relativeLocalPath));
        return ExitCode::InvalidOperation;
    }

    seedPath = localPath / _relativeLocalPath;
    SyncPath canonicalSeedPath;
    if (const IoError ioError = IoHelper::getWeakCanonicalPath(seedPath, canonicalSeedPath); ioError != IoError::Success) {
        LOGW_WARN(_logger, L"Error in IoHelper::getWeakCanonicalPath: " << Utility::formatIoError(seedPath, ioError));
        return ExitCode::SystemError;
    }
    SyncPath canonicalLocalPath;
    if (const IoError ioError = IoHelper::getWeakCanonicalPath(localPath, canonicalLocalPath); ioError != IoError::Success) {
        LOGW_WARN(_logger, L"Error in IoHelper::getWeakCanonicalPath: " << Utility::formatIoError(localPath, ioError));
        return ExitCode::SystemError;
    }
    if (!CommonUtility::isDescendantOrEqual(canonicalSeedPath, canonicalLocalPath)) {
        LOGW_WARN(_logger, L"The reported path is located outside of the sync root: " << Utility::formatSyncPath(seedPath));
        return ExitCode::InvalidOperation;
    }

    return ExitCode::Ok;
}

ExitInfo ErrorQuickResolveHardlinkJob::checkSeedItem(const SyncPath &seedPath, bool &seedExists) const {
    seedExists = false;

    ItemType itemType;
    if (!IoHelper::getItemType(seedPath, itemType)) {
        LOGW_WARN(_logger, L"Error in IoHelper::getItemType: " << Utility::formatIoError(seedPath, itemType.ioError));
        return ExitCode::SystemError;
    }
    // Symbolic links, junctions and aliases are never followed: only the regular file reported in the error can be removed.
    // This check comes first, as the ioError of a symbolic link whose target does not exist is IoError::NoSuchFileOrDirectory.
    if (itemType.linkType != LinkType::None) {
        LOGW_WARN(_logger, L"The reported path is a link: " << Utility::formatSyncPath(seedPath));
        return ExitCode::InvalidOperation;
    }
    if (itemType.ioError == IoError::NoSuchFileOrDirectory) {
        LOGW_WARN(_logger, L"The file does not exist anymore: " << Utility::formatSyncPath(seedPath));
        return ExitCode::Ok;
    }
    if (itemType.ioError != IoError::Success) {
        LOGW_WARN(_logger, L"Error in IoHelper::getItemType: " << Utility::formatIoError(seedPath, itemType.ioError));
        return ExitCode::SystemError;
    }
    if (itemType.nodeType != NodeType::File) {
        // Never remove a directory, as its whole content would be removed.
        LOGW_WARN(_logger, L"The reported path is not a file: " << Utility::formatSyncPath(seedPath));
        return ExitCode::InvalidOperation;
    }

    // The seed path still exists: check that it refers to the node reported in the error, so that no other item can be removed
    // by mistake.
    std::optional<NodeId> seedNodeId;
    if (ExitInfo exitInfo = getLocalNodeId(seedPath, seedNodeId); !exitInfo) {
        return exitInfo;
    }
    if (!seedNodeId) {
        LOGW_WARN(_logger, L"The file does not exist anymore: " << Utility::formatSyncPath(seedPath));
        return ExitCode::Ok;
    }
    if (*seedNodeId != _nodeId) {
        LOGW_WARN(_logger, L"The item located at " << Utility::formatSyncPath(seedPath)
                                                   << L" does not refer to the reported node " << CommonUtility::s2ws(_nodeId));
        return ExitCode::InvalidOperation;
    }

    seedExists = true;
    return ExitCode::Ok;
}

ExitInfo ErrorQuickResolveHardlinkJob::getLocalNodeId(const SyncPath &path, std::optional<NodeId> &nodeId) const {
    nodeId = std::nullopt;

    // The file status of a symbolic link is the one of the link itself: the link target is not taken into account.
    FileStat fileStat;
    IoError ioError = IoError::Success;
    if (!IoHelper::getFileStat(path, &fileStat, ioError, IoHelper::PathCheckOption::Insensitive)) {
        LOGW_WARN(_logger, L"Error in IoHelper::getFileStat: " << Utility::formatIoError(path, ioError));
        return ExitCode::SystemError;
    }
    if (ioError == IoError::NoSuchFileOrDirectory) {
        return ExitCode::Ok;
    }
    if (ioError != IoError::Success) {
        LOGW_WARN(_logger, L"Error in IoHelper::getFileStat: " << Utility::formatIoError(path, ioError));
        return ExitCode::SystemError;
    }

    nodeId = std::to_string(fileStat.inode);
    return ExitCode::Ok;
}

ExitInfo ErrorQuickResolveHardlinkJob::removeLinks(const std::shared_ptr<SyncPal> &syncPal, const DbNode &dbNode,
                                                   const SyncPath &seedPath) const {
    std::vector<SyncPath> linkPaths;
    if (ExitInfo exitInfo = getLinkPathsUnderSyncRoot(syncPal->localPath(), seedPath, linkPaths); !exitInfo) {
        return exitInfo;
    }

    // Check whether the local file is in sync with the database. This must be done before removing the node from the database,
    // as the check requires the node to be present.
    if (!syncPal->isLocalItemInSyncWithDb(seedPath)) {
        // The file content may differ from the remote version: save a copy into the rescue folder before removing the
        // hardlinks, so that the user does not lose any data.
        if (ExitInfo exitInfo = rescueFile(syncPal, dbNode, seedPath); !exitInfo) {
            return exitInfo;
        }
    }

    // Remove the item from the temporary blacklist, if any, so that the file system observer processes the deletion of the
    // links and the download of the file during the next synchronization.
    syncPal->removeItemFromTmpBlacklist(_nodeId, ReplicaSide::Local);
    if (dbNode.hasRemoteNodeId()) {
        syncPal->removeItemFromTmpBlacklist(dbNode.nodeIdRemote().value(), ReplicaSide::Remote);
    }

    return deleteLinks(syncPal, linkPaths, seedPath);
}

ExitInfo ErrorQuickResolveHardlinkJob::getLinkPathsUnderSyncRoot(const SyncPath &localPath, const SyncPath &seedPath,
                                                                 std::vector<SyncPath> &linkPaths) const {
    linkPaths.clear();

    // Enumerate all the existing paths of the file, starting from the path reported in the error. Any error fails the job, as
    // an incomplete list would leave links behind once the node is removed from the database.
    std::vector<SyncPath> hardlinkPaths;
    IoError ioError = IoError::Success;
    if (!IoHelper::getHardlinkPaths(seedPath, hardlinkPaths, ioError, localPath)) {
        LOGW_WARN(_logger, L"Error in IoHelper::getHardlinkPaths: " << Utility::formatIoError(seedPath, ioError));
        return ExitCode::SystemError;
    }

    // Keep only the links located under the sync root.
    for (const auto &path: hardlinkPaths) {
        if (CommonUtility::isDescendantOrEqual(path, localPath)) {
            linkPaths.push_back(path);
        } else {
            LOGW_DEBUG(_logger, L"Link located outside of the sync root, skipping: " << Utility::formatSyncPath(path));
        }
    }

    return ExitCode::Ok;
}

ExitInfo ErrorQuickResolveHardlinkJob::rescueFile(const std::shared_ptr<SyncPal> &syncPal, const DbNode &dbNode,
                                                  const SyncPath &seedPath) const {
    const SyncPath rescueFolderPath = syncPal->localPath() / FileRescuer::rescueFolderName();

    bool rescueFolderExists = false;
    IoError ioError = IoError::Unknown;
    (void) IoHelper::checkIfPathExists(rescueFolderPath, rescueFolderExists, ioError, IoHelper::PathCheckOption::Insensitive);
    if (ioError != IoError::Success) {
        LOGW_WARN(_logger, L"Failed to check rescue directory existence. Error: " << Utility::formatIoError(ioError));
        return ExitCode::SystemError;
    }
    if (!rescueFolderExists) {
        LocalCreateDirJob createRescueFolderJob(rescueFolderPath);
        if (ExitInfo exitInfo = createRescueFolderJob.runSynchronously(); !exitInfo) {
            LOGW_WARN(_logger,
                      L"Failed to create the rescue folder " << Utility::formatSyncPath(rescueFolderPath) << L": " << exitInfo);
            return exitInfo;
        }
    }

    ExitInfo copyExitInfo;
    SyncPath relativeDestinationPath;
    uint16_t counter = 0;
    do {
        const SyncName suffix =
                Str(" (") + Str2SyncName(std::to_string(counter)) + Str(")"); // TODO : use format when fully moved to c++20
        const SyncName filename =
                counter == 0 ? seedPath.filename().native() : seedPath.stem().native() + suffix + seedPath.extension().native();
        const SyncPath destinationPath = rescueFolderPath / filename;
        LocalCopyJob copyJob(seedPath, destinationPath);
        copyExitInfo = copyJob.runSynchronously();
        relativeDestinationPath = FileRescuer::rescueFolderName() / filename;
        counter++;
    } while (!copyExitInfo && copyExitInfo.cause() == ExitCause::FileExists);
    if (!copyExitInfo) {
        LOGW_WARN(_logger,
                  L"Failed to copy " << Utility::formatSyncPath(seedPath) << L" into the rescue folder: " << copyExitInfo);
        return copyExitInfo;
    }

    const Error error(_syncDbId, _nodeId, dbNode.hasRemoteNodeId() ? dbNode.nodeIdRemote().value() : NodeId(), dbNode.type(),
                      _relativeLocalPath, ConflictType::None, InconsistencyType::None, CancelType::FileRescued,
                      relativeDestinationPath);
    syncPal->addError(error);

    return ExitCode::Ok;
}

ExitInfo ErrorQuickResolveHardlinkJob::deleteLinks(const std::shared_ptr<SyncPal> &syncPal,
                                                   const std::vector<SyncPath> &linkPaths, const SyncPath &seedPath) const {
    // Delete the seed path last: if the deletion of a link fails, the job can be retried and the seed path is then still
    // available to enumerate the remaining links.
    std::vector<SyncPath> pathsToDelete = linkPaths;
    (void) std::stable_partition(pathsToDelete.begin(), pathsToDelete.end(),
                                 [&seedPath](const SyncPath &path) { return path != seedPath; });

    // Hard remove the links. Before each removal, check that the path still refers to the reported node, so that no other item
    // can be removed by mistake.
    for (const auto &path: pathsToDelete) {
        std::optional<NodeId> linkNodeId;
        if (ExitInfo exitInfo = getLocalNodeId(path, linkNodeId); !exitInfo) {
            return exitInfo;
        }
        if (linkNodeId != _nodeId) {
            // The link has disappeared or the item has been replaced: skip it, the next synchronization handles it.
            LOGW_WARN(_logger, L"Skip " << Utility::formatSyncPath(path) << L" as it does not refer to the reported node "
                                        << CommonUtility::s2ws(_nodeId));
            continue;
        }

        GenericLocalDeleteJob deleteJob(path, syncPal->cacheDirectory(), GenericLocalDeleteJob::ForceHardDelete::Yes);
        if (ExitInfo exitInfo = deleteJob.runSynchronously(); !exitInfo) {
            LOGW_WARN(_logger, L"Failed to delete " << Utility::formatSyncPath(path) << L": " << exitInfo);
            return exitInfo;
        }
    }

    return ExitCode::Ok;
}

ExitInfo ErrorQuickResolveHardlinkJob::deleteDbNode(const std::shared_ptr<SyncPal> &syncPal, const DbNode &dbNode) const {
    bool nodeFound = false;
    if (!syncPal->syncDb()->deleteNode(dbNode.nodeId(), nodeFound)) {
        LOGW_WARN(_logger, L"Error in SyncDb::deleteNode for DB node ID=" << dbNode.nodeId());
        return ExitCode::DbError;
    }
    if (!nodeFound) {
        LOGW_WARN(_logger, L"Node not found in the sync database for DB node ID=" << dbNode.nodeId());
    }

    return ExitCode::Ok;
}

ExitInfo ErrorQuickResolveHardlinkJob::deleteParmsDbError() const {
    // Remove the reported error from the parameters database, otherwise the GUI would keep displaying the error card after a
    // restart (see ErrorDeleteJob). The error has been checked by checkParmsDbError.
    bool errorFound = false;
    if (!ParmsDb::instance()->deleteError(_errorDbId, errorFound)) {
        LOG_WARN(_logger, "Error in ParmsDb::deleteError for errorDbId=" << _errorDbId);
        return ExitCode::DbError;
    }
    if (!errorFound) {
        LOG_INFO(_logger, "Error with errorDbId=" << _errorDbId << ": already removed from the database");
    }

    return ExitCode::Ok;
}

} // namespace KDC
