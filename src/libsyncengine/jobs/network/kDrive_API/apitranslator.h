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
#include "libsyncengine/update_detection/file_system_observer/snapshot/snapshotitem.h"

#include <map>

namespace KDC {

class ApiTranslator {
    public:
        ApiTranslator() = default;
        ~ApiTranslator() = default;

        [[nodiscard]] static ExitInfo translateV2ToV3(UserDbId userDbId, DriveId driveId, RemoteNodeId &remoteDirectoryId);
        [[nodiscard]] static ExitInfo translateV2ToV3(DriveDbId driveDbId, RemoteNodeId &remoteDirectoryId);
        // Translates an absolute API v3 path into the API v2 tree. The API v3 only sends absolute paths, starting with
        // the root separator:
        // - "/Private" is the root of the user's private space in the v3 tree. It translates to the empty path, which
        //   represents the v2 drive root;
        // - "/Private/<path>" translates to "<path>", a path relative to the v2 drive root;
        // - any other path is unchanged, as the v2 tree uses the same paths for the other roots ("/Common documents",
        //   "/Shared").
        static void translateV3ToV2(std::string &remotePath);
#if defined(KD_WINDOWS)
        static void translateV3ToV2(SyncName &remotePath);
#endif
        static void translateV3ToV2(QString &v3Path);
        [[nodiscard]] static ExitInfo translateV3ToV2(UserDbId userDbId, DriveId driveId, NodeId &remoteNodeId);
        [[nodiscard]] static ExitInfo translateV3ToV2(UserDbId userDbId, DriveId driveId,
                                                      RemoteNodeInfoList &v3RemoteNodeInfoList);
        [[nodiscard]] static ExitInfo getSpecialFolderRemoteId(UserDbId userDbId, DriveId driveId,
                                                               SpecialRemoteFolder specialFolder, RemoteNodeId &folderRemoteId);
        [[nodiscard]] static ExitInfo getSharedFolderSnapshotItem(UserDbId userDbId, DriveId driveId, RemoteSnapshotItem &item);
        [[nodiscard]] static ExitInfo getDriveDbId(DriveId driveId, DriveDbId &driveDbId);
        [[nodiscard]] static RemoteNodeId v2RootFolderRemoteId();

        static void clearSharedCache(const UserDbId userDbId, const DriveId driveId) {
            const CacheKey key{userDbId, driveId};
            const std::scoped_lock lock(_mutex);
            (void) _specialFolderRemoteIdsCache[SpecialRemoteFolder::Shared].erase(key);
            (void) _sharedFolderSnapshotItemCacheMap.erase(key);
        };

        static const SpecialFolderNames v3SpecialFolderNames;

    private:
        [[nodiscard]] static bool getDriveDbIds(DriveDbIdMap &driveIdMap);
        [[nodiscard]] static ExitInfo updateCache(UserDbId userDbId, DriveId driveId);

        using CacheKey = std::pair<UserDbId, DriveId>;
        using RemoteNodeIdCacheMap = std::map<CacheKey, RemoteNodeId>;
        using RemoteSpecialFoldersCacheMap = std::unordered_map<SpecialRemoteFolder, RemoteNodeIdCacheMap>;
        static RemoteSpecialFoldersCacheMap _specialFolderRemoteIdsCache;
        using SharedFolderSnapshotItemCacheMap = std::map<CacheKey, RemoteSnapshotItem>;
        static SharedFolderSnapshotItemCacheMap _sharedFolderSnapshotItemCacheMap;

        [[nodiscard]] static RemoteNodeId getValue(const CacheKey &key, const RemoteNodeIdCacheMap &cache);

        static std::mutex _mutex;
};

} // namespace KDC
