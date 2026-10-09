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

#include "utilityunlinkhardlinksjob.h"

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
#include <limits>

// Input parameters keys
static const auto inParamsSyncDbId = "syncDbId";
static const auto inParamsErrorDbId = "errorDbId";
static const auto inParamsNodeId = "nodeId";
static const auto inParamsPath = "path";

namespace KDC {

UtilityUnlinkHardlinksJob::UtilityUnlinkHardlinksJob(std::shared_ptr<CommManager> commManager, int32_t requestId,
                                                     const Poco::DynamicStruct &inParams,
                                                     std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::UTILITY_UNLINK_HARDLINKS;
}

ExitInfo UtilityUnlinkHardlinksJob::deserializeInputParms() {
    try {
        readParamValue(inParamsSyncDbId, _syncDbId);
        readParamValue(inParamsErrorDbId, _errorDbId);
        readParamValue(inParamsNodeId, _nodeId);

        CommString path;
        readParamValue(inParamsPath, path);
        _relativeLocalPath = CommonUtility::commString2SyncPath(path);
    } catch (const std::exception &e) {
        LOG_WARN(_logger, "Exception in UtilityUnlinkHardlinksJob::readParamValue: error=" << e.what());
        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo UtilityUnlinkHardlinksJob::process() {
    std::shared_ptr<SyncPal> syncPal;
    if (ExitInfo exitInfo = getSyncPal(_syncDbId, syncPal); !exitInfo) {
        return exitInfo;
    }

    UserActionScopedLock lock;
    if (syncPal != nullptr && !lock.tryLock(syncPal, std::chrono::milliseconds(userActionLockShortTimeoutMs))) {
        LOG_WARN(_logger, "Could not acquire user action lock for syncDbId="
                                  << _syncDbId << ". Another user action is running. Aborting UtilityUnlinkHardlinksJob.");
        return ExitCode::OperationCanceled;
    }

    if (ExitInfo exitInfo = unlinkHardlinks(syncPal); !exitInfo) {
        return exitInfo;
    }

    // Notify the GUI that the error has been resolved so that it can remove the corresponding error card.
    if (_commManager) {
        _commManager->sendGuiSignal(std::make_shared<SignalErrorRemovedJob>(_errorDbId));
    }

    return ExitCode::Ok;
}

ExitInfo UtilityUnlinkHardlinksJob::unlinkHardlinks(const std::shared_ptr<SyncPal> &syncPal) const {
    // Reject an outdated or inconsistent request before any change, so that the error removed at the end is the one of the
    // processed node.
    if (ExitInfo exitInfo = checkParmsDbError(); !exitInfo) {
        return exitInfo;
    }

    // Fetch the corresponding node in the sync database. Deleting this node removes both replicas, so the next synchronization
    // will see the remote file as a new item and will download it again as a standard file.
    DbNode dbNode;
    bool nodeFound = false;
    if (ExitInfo exitInfo = fetchFileDbNode(syncPal, dbNode, nodeFound); !exitInfo) {
        return exitInfo;
    }

    if (nodeFound) {
        if (ExitInfo exitInfo = removeLinksAndNode(syncPal, dbNode); !exitInfo) {
            return exitInfo;
        }
    } else {
        // The node is no longer in the sync database: either a previous run of this action has already removed it (its last
        // cleanup step failed) or a synchronization has already handled the item. There is nothing left to remove on the file
        // system: only complete the cleanup, so that a failed resolution can be retried and the error card removed.
        LOGW_INFO(_logger, L"Node not found in the sync database for node id " << CommonUtility::s2ws(_nodeId));
    }

    // Remove the items from the temporary blacklist, if any, only now that the node is no longer in the sync database: while it
    // is still present, the blacklist prevents the file system observer from generating a delete operation for the removed
    // links, which would propagate the deletion to the remote replica instead of allowing the remote file to be downloaded
    // again.
    syncPal->removeItemFromTmpBlacklist(_nodeId, ReplicaSide::Local);
    if (dbNode.hasRemoteNodeId()) {
        syncPal->removeItemFromTmpBlacklist(dbNode.nodeIdRemote().value(), ReplicaSide::Remote);
    }

    if (ExitInfo exitInfo = deleteParmsDbError(); !exitInfo) {
        return exitInfo;
    }

    LOG_INFO(_logger,
             "Unlink hardlinks done for syncDbId=" << _syncDbId << ", errorDbId=" << _errorDbId << ", nodeId=" << _nodeId);

    return ExitCode::Ok;
}

ExitInfo UtilityUnlinkHardlinksJob::removeLinksAndNode(const std::shared_ptr<SyncPal> &syncPal, const DbNode &dbNode) const {
    SyncPath seedPath;
    if (ExitInfo exitInfo = getSeedPath(syncPal->localPath(), seedPath); !exitInfo) {
        return exitInfo;
    }

    bool seedExists = false;
    if (ExitInfo exitInfo = checkSeedItem(seedPath, seedExists); !exitInfo) {
        return exitInfo;
    }

    std::vector<SyncPath> linkPaths;
    if (seedExists) {
        if (ExitInfo exitInfo = getLinkPathsUnderSyncRoot(syncPal->localPath(), seedPath, linkPaths); !exitInfo) {
            return exitInfo;
        }
    } else {
        // The reported path does not exist anymore, but the file may still have links under the sync root, e.g. if the
        // reported link has been removed or if the file has been moved: search them by node id and use one of them as the
        // seed path.
        if (ExitInfo exitInfo = findLinkPathsByNodeId(syncPal->localPath(), linkPaths); !exitInfo) {
            return exitInfo;
        }
        if (!linkPaths.empty()) {
            seedPath = linkPaths.front();
            LOGW_INFO(_logger, L"Link of the reported node found at " << Utility::formatSyncPath(seedPath));
        }
    }

    // If the file has no link left under the sync root, only remove the node from the database so that the file is
    // downloaded again.
    if (!linkPaths.empty()) {
        if (ExitInfo exitInfo = removeLinks(syncPal, dbNode, seedPath, linkPaths); !exitInfo) {
            return exitInfo;
        }
    }

    if (ExitInfo exitInfo = deleteDbNode(syncPal, dbNode); !exitInfo) {
        return exitInfo;
    }

    return ExitInfo(ExitCode::Ok);
}

ExitInfo UtilityUnlinkHardlinksJob::checkParmsDbError() const {
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

ExitInfo UtilityUnlinkHardlinksJob::fetchFileDbNode(const std::shared_ptr<SyncPal> &syncPal, DbNode &dbNode,
                                                    bool &nodeFound) const {
    nodeFound = false;

    bool found = false;
    if (!syncPal->syncDb()->node(ReplicaSide::Local, _nodeId, dbNode, found)) {
        LOGW_WARN(_logger, L"Error in SyncDb::node for node id " << CommonUtility::s2ws(_nodeId));
        return ExitCode::DbError;
    }
    if (!found) {
        // The node is no longer in the sync database: the caller only completes the cleanup, so that a resolution whose final
        // cleanup step failed can be retried.
        return ExitCode::Ok;
    }
    if (dbNode.type() != NodeType::File) {
        // Only files can have hardlinks: never remove a directory, as its whole content would be removed.
        LOGW_WARN(_logger, L"The node with node id " << CommonUtility::s2ws(_nodeId) << L" is not a file");
        return ExitCode::InvalidOperation;
    }

    nodeFound = true;
    return ExitCode::Ok;
}

ExitInfo UtilityUnlinkHardlinksJob::getSeedPath(const SyncPath &localPath, SyncPath &seedPath) const {
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

ExitInfo UtilityUnlinkHardlinksJob::checkSeedItem(const SyncPath &seedPath, bool &seedExists) const {
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

ExitInfo UtilityUnlinkHardlinksJob::getLocalNodeId(const SyncPath &path, std::optional<NodeId> &nodeId) const {
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

ExitInfo UtilityUnlinkHardlinksJob::selectSeedPath(const std::vector<SyncPath> &linkPaths, SyncPath &seedPath,
                                                   bool &seedFound) const {
    // Use the current seed path as long as it still refers to the reported node, or the first link that does.
    seedFound = false;
    for (const auto &path: linkPaths) {
        std::optional<NodeId> linkNodeId;
        if (ExitInfo exitInfo = getLocalNodeId(path, linkNodeId); !exitInfo) {
            return exitInfo;
        }
        if (linkNodeId == _nodeId) {
            if (path != seedPath) {
                LOGW_INFO(_logger, L"Link of the reported node found at " << Utility::formatSyncPath(path));
                seedPath = path;
            }
            seedFound = true;
            break;
        }
        LOGW_WARN(_logger, L"Skip " << Utility::formatSyncPath(path) << L" as it does not refer to the reported node "
                                    << CommonUtility::s2ws(_nodeId));
    }

    return ExitCode::Ok;
}

ExitInfo UtilityUnlinkHardlinksJob::removeLinks(const std::shared_ptr<SyncPal> &syncPal, const DbNode &dbNode, SyncPath &seedPath,
                                                const std::vector<SyncPath> &linkPaths) const {
    // The directory entry of the seed path may have been replaced since the item has been checked: only a link that still
    // refers to the reported node can be used for the consistency check and the rescue copy. If no link refers to it anymore,
    // there is nothing left to remove: the caller removes the node from the database so that the file is downloaded again.
    bool seedFound = false;
    if (ExitInfo exitInfo = selectSeedPath(linkPaths, seedPath, seedFound); !exitInfo) {
        return exitInfo;
    }
    if (!seedFound) {
        LOGW_INFO(_logger, L"No link of the reported node is left under the sync root");
        return ExitCode::Ok;
    }

    // Check whether the local file is in sync with the database. This must be done before removing the node from the database,
    // as the check requires the node to be present. A seed path found by node id that is not located at the path stored in the
    // database is considered as not in sync: a copy of the file is then saved into the rescue folder.
    bool inSync = syncPal->isLocalItemInSyncWithDb(seedPath);
    if (inSync) {
        // On Windows, the check relies on the size and the dates cached in the directory entry of the seed path, which NTFS
        // does not update when the file is modified through another hardlink. The file is considered as not in sync if these
        // values are outdated, or if this cannot be checked.
        bool upToDate = false;
        IoError ioError = IoError::Success;
        if (!IoHelper::checkIfFileStatIsUpToDate(seedPath, upToDate, ioError) || ioError != IoError::Success) {
            LOGW_WARN(_logger, L"Error in IoHelper::checkIfFileStatIsUpToDate: " << Utility::formatIoError(seedPath, ioError));
        }
        if (!upToDate) {
            LOGW_INFO(_logger, L"The file status may be outdated, the file is considered as not in sync: "
                                       << Utility::formatSyncPath(seedPath));
            inSync = false;
        }
    }

    if (!inSync) {
        // The file content may differ from the remote version: save a copy into the rescue folder before removing the
        // hardlinks, so that the user does not lose any data.
        if (ExitInfo exitInfo = rescueFile(syncPal, dbNode, seedPath); !exitInfo) {
            return exitInfo;
        }
    }

    return deleteLinks(syncPal, linkPaths, seedPath);
}

ExitInfo UtilityUnlinkHardlinksJob::getLinkPathsUnderSyncRoot(const SyncPath &localPath, const SyncPath &seedPath,
                                                              std::vector<SyncPath> &linkPaths) const {
    linkPaths.clear();

    // Enumerate all the existing paths of the file located under the sync root, starting from the path reported in the error.
    // Any error fails the job, as an incomplete list would leave links behind once the node is removed from the database.
    IoError ioError = IoError::Success;
    if (!IoHelper::getHardlinkPaths(seedPath, linkPaths, ioError, localPath)) {
        LOGW_WARN(_logger, L"Error in IoHelper::getHardlinkPaths: " << Utility::formatIoError(seedPath, ioError));
        linkPaths.clear();
        return ExitCode::SystemError;
    }

    return ExitCode::Ok;
}

ExitInfo UtilityUnlinkHardlinksJob::findLinkPathsByNodeId(const SyncPath &localPath, std::vector<SyncPath> &linkPaths) const {
    linkPaths.clear();

    // Any error fails the job, as an incomplete list would leave links behind once the node is removed from the database.
    IoError ioError = IoError::Success;
    if (!IoHelper::getPathsWithNodeId(localPath, _nodeId, linkPaths, ioError)) {
        LOGW_WARN(_logger, L"Error in IoHelper::getPathsWithNodeId: " << Utility::formatIoError(localPath, ioError));
        linkPaths.clear();
        return ExitCode::SystemError;
    }

    return ExitCode::Ok;
}

ExitInfo UtilityUnlinkHardlinksJob::rescueFile(const std::shared_ptr<SyncPal> &syncPal, const DbNode &dbNode,
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
    // Bound the number of attempts, so that the job fails instead of looping forever if every candidate name is already taken
    // in the rescue folder.
    constexpr uint32_t maxCounter = std::numeric_limits<uint16_t>::max();
    for (uint32_t counter = 0; counter <= maxCounter; counter++) {
        const SyncName suffix =
                Str(" (") + Str2SyncName(std::to_string(counter)) + Str(")"); // TODO : use format when fully moved to c++20
        const SyncName filename =
                counter == 0 ? seedPath.filename().native() : seedPath.stem().native() + suffix + seedPath.extension().native();
        const SyncPath destinationPath = rescueFolderPath / filename;
        LocalCopyJob copyJob(seedPath, destinationPath);
        copyExitInfo = copyJob.runSynchronously();
        relativeDestinationPath = FileRescuer::rescueFolderName() / filename;
        if (copyExitInfo || copyExitInfo.cause() != ExitCause::FileExists) break;
    }
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

ExitInfo UtilityUnlinkHardlinksJob::deleteLinks(const std::shared_ptr<SyncPal> &syncPal, const std::vector<SyncPath> &linkPaths,
                                                const SyncPath &seedPath) const {
    // Delete the seed path last: if the deletion of another link fails, the seed path still refers to the file when the job is
    // retried.
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

ExitInfo UtilityUnlinkHardlinksJob::deleteDbNode(const std::shared_ptr<SyncPal> &syncPal, const DbNode &dbNode) const {
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

ExitInfo UtilityUnlinkHardlinksJob::deleteParmsDbError() const {
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
