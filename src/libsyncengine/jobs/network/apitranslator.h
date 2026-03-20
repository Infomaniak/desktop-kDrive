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

#pragma once

#include "libcommon/info/nodeinfo.h"

namespace KDC {

class ApiTranslator {
    public:
        ApiTranslator();
        ~ApiTranslator();

        static void translateV2ToV3(DriveDbId driveDbId, NodeId &remoteDirectoryId);
        static void translateV3ToV2(SyncPath &remotePath);
        static void translateV3ToV2(DriveDbId driveDbId, NodeId &remoteNodeId);
        static void translateV3ToV2(DriveDbId driveDbId, NodeInfoList &v3NodeInfoList);
        static RemoteNodeId getUserPrivateFolderRemoteId(DriveDbId driveDbId);
        static RemoteNodeId getCommonDocumentsRemoteId(DriveDbId driveDbId);
        static RemoteNodeId getSharedRemoteId(DriveDbId driveDbId);

        static DriveDbId getDriveDbId(DriveId driveId);

        static const RemoteNodeId v2RootFolderRemoteId;

    private:
        static DriveId getDriveId(DriveDbId driveDbId);
        static bool getDriveDbIds(DriveDbIdMap &driveIdMap);
        static void updateCache(DriveDbId driveDbId);

        using RemoteNodeIdCacheMap = std::unordered_map<DriveDbId, RemoteNodeId>;
        static RemoteNodeIdCacheMap _rootNodeIdCache;
        static RemoteNodeIdCacheMap _commonDocumentsNodeIdCache;
        static RemoteNodeIdCacheMap _sharedNodeIdCache;

        static RemoteNodeId getValue(const DriveDbId driveDbId, const RemoteNodeIdCacheMap &cache);

        static std::mutex _mutex;
};

} // namespace KDC
