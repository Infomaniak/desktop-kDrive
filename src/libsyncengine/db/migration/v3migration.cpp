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

#include "v3migration.h"

#include "../syncdb.h"

#include "libcommonserver/io/cachedirectory.h"
#include "libcommonserver/io/filestat.h"

#include "libsyncengine/jobs/network/kDrive_API/apitranslator.h"
#include "libsyncengine/propagation/executor/filerescuer.h"

namespace KDC {

V3Migration::V3Migration(std::shared_ptr<SyncDb> syncDbPtr) :
    Migration(syncDbPtr) {}


void V3Migration::removeTempPrivateDir(const SyncPath &privateTmpLocalPath) const {
    auto deletionError = IoError::Success;
    if (!IoHelper::deleteItem(privateTmpLocalPath, deletionError) || deletionError != IoError::Success) {
        LOGW_WARN(logger(), L"Error in IoHelper::deleteItem: " << Utility::formatIoError(privateTmpLocalPath, deletionError));
    }
}

bool V3Migration::moveLocalItemsToTmpPrivateDir(const SyncPath &localSyncDirPath, const SyncPath &privateTmpLocalPath) const {
    IoHelper::DirectoryIterator dir;
    auto iteratorError = IoError::Success;
    if (!IoHelper::getDirectoryIterator(localSyncDirPath, false, iteratorError, dir) || iteratorError != IoError::Success) {
        LOGW_WARN(logger(), L"Error in IoHelper::getRecursiveDirectoryIterator: "
                                    << Utility::formatIoError(localSyncDirPath, iteratorError));

        removeTempPrivateDir(privateTmpLocalPath);

        return iteratorError == IoError::NoSuchFileOrDirectory;
    }

    const SyncNameSet excludedItemNames{
            Str2SyncName(ApiTranslator::v3SpecialFolderNames.at(ApiTranslator::SpecialFolder::CommonDocuments)),
            Str2SyncName(ApiTranslator::v3SpecialFolderNames.at(ApiTranslator::SpecialFolder::Shared)),
            Str2SyncName(std::string(CacheDirectory::name())), FileRescuer::rescueFolderName().filename(),
            privateTmpLocalPath.filename()};

    DirectoryEntry entry;
    bool endOfDirectory = false;
    while (dir.next(entry, endOfDirectory, iteratorError) && !endOfDirectory) {
        if (excludedItemNames.contains(entry.path().filename())) continue;

        auto moveItemError = IoError::Success;
        if (!IoHelper::moveItem(entry.path(), privateTmpLocalPath / entry.path().filename(), moveItemError) ||
            moveItemError != IoError::Success) {
            LOGW_WARN(logger(), L"Error in IoHelper::moveItem: " << Utility::formatIoError(entry.path(), moveItemError));

            return false;
        }
    }

    LOGW_INFO(logger(),
              L"All high level items have moved to " << Utility::formatSyncPath(privateTmpLocalPath) << L" successfully.");

    return true;
}

bool V3Migration::renameTempPrivateDir(const SyncPath &privateTmpLocalPath, const SyncPath &privateLocalPath) const {
    if (auto renamingError = IoError::Success;
        !IoHelper::renameItem(privateTmpLocalPath, privateLocalPath, renamingError) || renamingError != IoError::Success) {
        LOGW_WARN(logger(), L"Error in IoHelper::renameItem for target "
                                    << Utility::formatSyncPath(privateTmpLocalPath)
                                    << Utility::formatIoError(privateTmpLocalPath, renamingError));

        return false;
    }

    LOGW_INFO(logger(), L"Temporary Private folder " << Utility::formatSyncPath(privateTmpLocalPath) << L" has been renamed to "
                                                     << Utility::formatSyncPath(privateLocalPath) << L" successfully.");

    return true;
}

bool V3Migration::migrateLocalItemsToPrivateDir() {
    Sync sync;
    bool found = false;

    if (!ParmsDb::instance()->selectSync(syncDb()->dbPath(), sync, found)) {
        LOG_WARN(logger(), "Error in ParmsDb::selectSync");
        return false;
    }
    if (!found) {
        LOGW_WARN(logger(), L"Sync DB with " << Utility::formatSyncPath(syncDb()->dbPath()) << L" not found.");
        return false;
    }

    const SyncPath &localSyncDirPath = sync.localPath();

    if (!syncDb()->rootNode().nodeIdRemote()) {
        LOGW_INFO(logger(), L"Sync with " << Utility::formatSyncPath(sync.localPath()) << L" has no target node id. Aborting.");
        return false;
    }

    if (!sync.targetPath().empty() && *syncDb()->rootNode().nodeIdRemote() != ApiTranslator::v2RootFolderRemoteId()) {
        LOGW_INFO(logger(), L"Sync with " << Utility::formatSyncPath(sync.localPath())
                                          << L" is a non-root advanced sync. No Sync DB upgrade to do.");
        return true;
    }

    bool exists = false;

    if (auto existenceCheckError = IoError::Success;
        !IoHelper::checkIfPathExists(localSyncDirPath, exists, existenceCheckError, IoHelper::PathCheckOption::Insensitive)) {
        LOGW_WARN(logger(),
                  L"Error in IoHelper::checkIfPathExists" << Utility::formatIoError(localSyncDirPath, existenceCheckError));
        return false;
    }
    if (!exists) {
        LOGW_INFO(logger(), L"The synchronisation folder " << Utility::formatSyncPath(localSyncDirPath)
                                                           << L" does not exist anymore. No Sync DB upgrade to do.");
        return true;
    }


    const std::string randomSuffix = CommonUtility::generateRandomStringAlphaNum();
    const auto privateTmpLocalPath =
            localSyncDirPath / (ApiTranslator::v3SpecialFolderNames.at(ApiTranslator::SpecialFolder::Private) +
                                "_kDrive_v3_upgrade_" + randomSuffix);


    // Create the local "Private" folder with a random suffix to avoid conflict with an existing directory.
    if (auto privateCreationError = IoError::Success;
        !IoHelper::createDirectory(privateTmpLocalPath, false, privateCreationError) ||
        privateCreationError != IoError::Success) {
        LOGW_WARN(logger(),
                  L"Error in IoHelper::createDirectory: " << Utility::formatIoError(privateTmpLocalPath, privateCreationError));
        return false;
    }

    if (!moveLocalItemsToTmpPrivateDir(localSyncDirPath, privateTmpLocalPath)) {
        LOGW_WARN(logger(), L"Error in moveLocalItemsToTmpPrivateDir.");
        return false;
    }

    const auto privateLocalPath =
            localSyncDirPath / ApiTranslator::v3SpecialFolderNames.at(ApiTranslator::SpecialFolder::Private);

    if (!renameTempPrivateDir(privateTmpLocalPath, privateLocalPath)) {
        LOGW_WARN(logger(), L"Error in renameTempPrivateDir.");
        return false;
    }

    LOGW_INFO(logger(), L"Successful migration of high level items to local Private folder for sync with "
                                << Utility::formatSyncPath(localSyncDirPath));

    if (!updateParentNodeIdsOfRootChildren(sync.driveDbId(), privateLocalPath)) {
        LOGW_WARN(logger(), L"Error in updateParentNodeIdsOfRootChildren.");
        return false;
    }

    LOGW_INFO(logger(),
              L"Successful overall migration to Private folder for sync with " << Utility::formatSyncPath(localSyncDirPath));

    return true;
}

bool V3Migration::updateParentNodeIds(const DbNodeId parentNodeId) {
    const char *requestId = "update_parent_node_ids";

    if (const char *query =
                "UPDATE node SET parentNodeId=?1 WHERE parentNodeId = 1 AND nameLocal NOT IN ('Common documents', "
                "'Shared') AND nodeId <> ?2;";
        !syncDb()->createAndPrepareRequest(requestId, query))
        return false;

    const auto lock = syncDb()->lock();
    syncDb()->invalidateCache();

    auto errId = -1;
    std::string error;

    LOG_IF_FAIL(logger(), syncDb()->queryBindValue(requestId, 1, parentNodeId));
    LOG_IF_FAIL(logger(), syncDb()->queryBindValue(requestId, 2, parentNodeId));
    if (!syncDb()->queryExec(requestId, errId, error)) {
        LOG_WARN(logger(), "Error running query: " << requestId);
        syncDb()->queryFree(requestId);

        return false;
    }

    syncDb()->queryFree(requestId);

    return true;
}

bool V3Migration::getPrivateDirRemoteNodeId(const DriveDbId driveDbId, RemoteNodeId &privateDirRemoteNodeId) {
    privateDirRemoteNodeId = {};
    if (auto exitInfo =
                ApiTranslator::getSpecialFolderRemoteId(driveDbId, ApiTranslator::SpecialFolder::Private, privateDirRemoteNodeId);
        !exitInfo) {
        LOGW_WARN(logger(), L"Error in ApiTranslator::getSpecialFolderRemoteId for Private folder.");
        return false;
    }

    return true;
}

bool V3Migration::insertPrivateDirNode(const DriveDbId driveDbId, const SyncPath &localPrivateDirPath,
                                       DbNodeId &privateDirDbNodeId) {
    privateDirDbNodeId = 0;
    DbNode privateDbNode;

    privateDbNode.setParentNodeId(syncDb()->rootNode().nodeId());
    const auto privateDirName = Str2SyncName(ApiTranslator::v3SpecialFolderNames.at(ApiTranslator::SpecialFolder::Private));
    privateDbNode.setNameLocal(privateDirName);
    privateDbNode.setNameRemote(privateDirName);
    privateDbNode.setType(NodeType::Directory);

    NodeId privateDirLocalNodeId;
    if (!IoHelper::getNodeId(localPrivateDirPath, privateDirLocalNodeId) || privateDirLocalNodeId.empty()) {
        LOGW_WARN(logger(), L"Error in IoHelper::getNodeId for Private folder.");
        return false;
    }

    privateDbNode.setNodeIdLocal(privateDirLocalNodeId);

    RemoteNodeId privateDirRemoteNodeId;
    if (!getPrivateDirRemoteNodeId(driveDbId, privateDirRemoteNodeId)) return false;

    privateDbNode.setNodeIdRemote(privateDirRemoteNodeId);

    // Retrieve creation and last modification dates from the local directory.
    FileStat fileStat;
    if (auto ioError = IoError::Unknown;
        !IoHelper::getFileStat(localPrivateDirPath, &fileStat, ioError, IoHelper::PathCheckOption::Insensitive) ||
        ioError != IoError::Success) {
        LOGW_WARN(logger(), L"Failed to get FileStat for " << Utility::formatSyncPath(localPrivateDirPath) << L": " << ioError);
    }
    privateDbNode.setCreated(fileStat.creationTime);
    privateDbNode.setLastModifiedLocal(fileStat.modificationTime);

    if (bool constraintError = false; !syncDb()->insertNode(privateDbNode, privateDirDbNodeId, constraintError)) {
        LOGW_WARN(logger(), L"Error inserting Private directory node into SyncDb.");
        return false;
    }

    LOGW_INFO(logger(), L"Successful insertion of Private directory node with DB ID "
                                << privateDirDbNodeId << L" and local node ID " << CommonUtility::s2ws(privateDirLocalNodeId)
                                << L" for Private folder at " << Utility::formatSyncPath(localPrivateDirPath) << L".");

    return true;
}

bool V3Migration::updateParentNodeIdsOfRootChildren(const DriveDbId driveDbId, const SyncPath &localPrivateDirPath) {
    std::vector<DbNodeId> rootChildrenDbIds;

    DbNodeId privateNodeDbId = 0;
    if (!insertPrivateDirNode(driveDbId, localPrivateDirPath, privateNodeDbId)) {
        LOGW_WARN(logger(), L"Error in insertPrivateDirNode.");
        return false;
    }

    if (!updateParentNodeIds(privateNodeDbId)) {
        LOGW_WARN(logger(), L"Error in updateParentNodeIds.");
        return false;
    }

    LOG_INFO(logger(), "Successful update of parent node ids for all " << rootChildrenDbIds.size() << " root children.");

    return true;
}

bool V3Migration::migrate(const std::string &dbFromVersionNumber) {
    if (!CommonUtility::isVersionLower(dbFromVersionNumber, "4.0.1")) return true;

    LOG_DEBUG(logger(), "Migration to Private folder for version < 4.0.1 of Sync DB");

    if (!migrateLocalItemsToPrivateDir()) {
        LOG_WARN(logger(), "Error in V3Migration::migrateLocalItemsToPrivateDir. Migration failed.");

        return false;
    }

    LOG_INFO(logger(), "Migration of synchronized items to Private folder successfully completed.");

    return true;
}
} // namespace KDC
