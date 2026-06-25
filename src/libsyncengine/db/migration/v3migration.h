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

#pragma once

#include "migration.h"

#include <memory>

namespace KDC {

class SyncDb;

class V3Migration : public Migration {
    public:
        V3Migration(std::shared_ptr<SyncDb> synDbPtr);
        virtual ~V3Migration() = default;

        bool migrate(const std::string &dbFromVersionNumber) override;

    private:
        // Remove the temporary Private folder introduced with the backend API v3.
        void removeTempPrivateDir(const SyncPath &privateTmpLocalPath) const;

        bool renameTempPrivateDir(const SyncPath &privateTmpLocalPath, const SyncPath &privateLocalPath) const;

        // Move local items to the Private folder introduced with the backend API v3.
        bool moveLocalItemsToTmpPrivateDir(const SyncPath &localSyncDirPath, const SyncPath &privateTmpLocalPath) const;

        // Move local items to the Private folder introduced with the backend API v3.
        // Update accordingly the parent node IDs the of moved items in the DB.
        bool migrateLocalItemsToPrivateDir();

        // Update the parent node DB ID of the children of the root node with the DB ID of the Private folder.
        bool updateParentNodeIdsOfRootChildren(DriveDbId driveDbId, const SyncPath &localPrivateDirPath);

        bool updateParentNodeIds(DbNodeId parentNodeId);
        virtual bool getPrivateDirRemoteNodeId(const DriveDbId driveDbId, RemoteNodeId &privateDirRemoteNodeId);
        bool insertPrivateDirNode(DriveDbId driveDbId, const SyncPath &localPrivateDirPath, DbNodeId &privateDirDbNodeId);

        friend class TestV3Migration;
};

} // namespace KDC
