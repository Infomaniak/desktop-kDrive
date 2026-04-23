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

#include "libcommon/info/nodeinfo.h"

namespace KDC {

class ApiTranslator {
    public:
        ApiTranslator() = default;
        ~ApiTranslator() = default;

        static ExitInfo translateV2ToV3(UserDbId userDbId, DriveId driveId, NodeId &remoteDirectoryId);
        static void translateV3ToV2(SyncPath &remotePath);
        static ExitInfo translateV3ToV2(UserDbId userDbId, DriveId driveId, NodeId &remoteNodeId);
        static ExitInfo translateV3ToV2(UserDbId userDbId, DriveId driveId, RemoteNodeInfoList &v3RemoteNodeInfoList);

        static ExitInfo getSpecialFolderRemoteId(UserDbId userDbId, DriveId driveId, SpecialFolder specialFolder,
                                                 RemoteNodeId &folderRemoteId);

        static ExitInfo getDriveDbId(DriveId driveId, DriveDbId &driveDbId);

        static RemoteNodeId v2RootFolderRemoteId();

        static const SpecialFolderNames v3SpecialFolderNames;

    private:
        static bool getDriveDbIds(DriveDbIdMap &driveIdMap);
        static ExitInfo updateCache(UserDbId userDbId, DriveId driveId);

        using RemoteNodeIdCacheMap = std::unordered_map<DriveDbId, RemoteNodeId>;
        using RemoteSpecialFoldersCacheMap = std::unordered_map<SpecialFolder, RemoteNodeIdCacheMap>;
        static RemoteSpecialFoldersCacheMap _specialFolderRemoteIdsCache;

        static RemoteNodeId getValue(DriveId driveId, const RemoteNodeIdCacheMap &cache);

        static std::mutex _mutex;
};

} // namespace KDC
