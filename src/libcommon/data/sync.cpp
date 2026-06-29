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

#include "data/sync.h"

#include "utility/utility.h"

static const auto syncInfoDbId = "dbId";
static const auto syncInfoDriveDbId = "driveDbId";
static const auto syncInfoLocalPath = "localPath";
static const auto syncInfoLocalNodeId = "localNodeId";
static const auto syncInfoTargetPath = "targetPath";
static const auto syncInfoTargetNodeId = "targetNodeId";
static const auto syncInfoPaused = "paused";
static const auto syncInfoSupportVfs = "supportVfs";
static const auto syncInfoVirtualFileMode = "virtualFileMode";
static const auto syncInfoNotificationsDisabled = "notificationsDisabled";
static const auto syncInfoDbPath = "dbPath";
static const auto syncInfoHasFullyCompleted = "hasFullyCompleted";
static const auto syncInfoNavigationPaneClsid = "navigationPaneClsid";
static const auto syncInfoListingCursor = "listingCursor";
static const auto syncInfoListingCursorTimestamp = "listingCursorTimestamp";

namespace KDC {

Sync::Sync(const SyncDbId dbId, const DriveDbId driveDbId, const std::filesystem::path &localPath, const NodeId &localNodeId,
           const std::filesystem::path &targetPath, const NodeId &targetNodeId, const bool paused, const bool supportVfs,
           const VirtualFileMode virtualFileMode, const bool notificationsDisabled, const std::filesystem::path &dbPath,
           const bool hasFullyCompleted, const std::string &navigationPaneClsid, const std::string &listingCursor,
           const int64_t listingCursorTimestamp) :
    _dbId(dbId),
    _driveDbId(driveDbId),
    _localPath(localPath),
    _localNodeId(localNodeId),
    _targetPath(targetPath),
    _targetNodeId(targetNodeId),
    _paused(paused),
    _supportVfs(supportVfs),
    _virtualFileMode(virtualFileMode),
    _notificationsDisabled(notificationsDisabled),
    _dbPath(dbPath),
    _hasFullyCompleted(hasFullyCompleted),
    _navigationPaneClsid(navigationPaneClsid),
    _listingCursor(listingCursor),
    _listingCursorTimestamp(listingCursorTimestamp) {}

void Sync::toDynamicStruct(Poco::DynamicStruct &dstruct) const {
    CommonUtility::writeValueToStruct(dstruct, syncInfoDbId, dbId());
    CommonUtility::writeValueToStruct(dstruct, syncInfoDriveDbId, driveDbId());
    CommonUtility::writeValueToStruct(dstruct, syncInfoLocalPath, CommonUtility::syncPath2CommString(localPath()));
    // CommonUtility::writeValueToStruct(dstruct, syncInfoLocalNodeId, CommonUtility::str2CommString(localNodeId()));
    CommonUtility::writeValueToStruct(dstruct, syncInfoTargetPath, CommonUtility::syncPath2CommString(targetPath()));
    CommonUtility::writeValueToStruct(dstruct, syncInfoTargetNodeId, CommonUtility::str2CommString(targetNodeId()));
    // CommonUtility::writeValueToStruct(dstruct, syncInfoPaused, paused());
    CommonUtility::writeValueToStruct(dstruct, syncInfoSupportVfs, supportVfs());
    CommonUtility::writeValueToStruct(dstruct, syncInfoVirtualFileMode, virtualFileMode());
    // CommonUtility::writeValueToStruct(dstruct, syncInfoNotificationsDisabled, notificationsDisabled());
    // CommonUtility::writeValueToStruct(dstruct, syncInfoDbPath, CommonUtility::syncPath2CommString(dbPath()));
    // CommonUtility::writeValueToStruct(dstruct, syncInfoHasFullyCompleted, hasFullyCompleted());
    CommonUtility::writeValueToStruct(dstruct, syncInfoNavigationPaneClsid, CommonUtility::str2CommString(navigationPaneClsid()));
    // CommonUtility::writeValueToStruct(dstruct, syncInfoListingCursor, CommonUtility::str2CommString(listingCursor()));
    // CommonUtility::writeValueToStruct(dstruct, syncInfoListingCursorTimestamp, listingCursorTimestamp());
}

void Sync::fromDynamicStruct(const Poco::DynamicStruct &dstruct) {
    SyncDbId syncDbId = 0;
    CommonUtility::readValueFromStruct(dstruct, syncInfoDbId, syncDbId);
    setDbId(syncDbId);

    DriveDbId driveDbId = 0;
    CommonUtility::readValueFromStruct(dstruct, syncInfoDriveDbId, driveDbId);
    setDriveDbId(driveDbId);

    CommString localPath;
    CommonUtility::readValueFromStruct(dstruct, syncInfoLocalPath, localPath);
    setLocalPath(CommonUtility::commString2SyncPath(localPath));

    // CommString localNodeId;
    // CommonUtility::readValueFromStruct(dstruct, syncInfoLocalNodeId, localNodeId);
    // setLocalNodeId(CommonUtility::commString2Str(localNodeId));

    CommString targetPath;
    CommonUtility::readValueFromStruct(dstruct, syncInfoTargetPath, targetPath);
    setTargetPath(CommonUtility::commString2SyncPath(targetPath));

    CommString targetNodeId;
    CommonUtility::readValueFromStruct(dstruct, syncInfoTargetNodeId, targetNodeId);
    setTargetNodeId(CommonUtility::commString2Str(targetNodeId));

    // bool paused = false;
    // CommonUtility::readValueFromStruct(dstruct, syncInfoPaused, paused);
    // setPaused(paused);

    CommonUtility::readValueFromStruct(dstruct, syncInfoSupportVfs, _supportVfs);
    CommonUtility::readValueFromStruct(dstruct, syncInfoVirtualFileMode, _virtualFileMode);

    // bool notificationsDisabled = false;
    // CommonUtility::readValueFromStruct(dstruct, syncInfoNotificationsDisabled, notificationsDisabled);
    // setNotificationsDisabled(notificationsDisabled);

    // CommString dbPath;
    // CommonUtility::readValueFromStruct(dstruct, syncInfoDbPath, dbPath);
    // setDbPath(CommonUtility::commString2SyncPath(dbPath));

    // CommonUtility::readValueFromStruct(dstruct, syncInfoHasFullyCompleted, _hasFullyCompleted);

    CommString navigationPaneClsid;
    CommonUtility::readValueFromStruct(dstruct, syncInfoNavigationPaneClsid, navigationPaneClsid);
    setNavigationPaneClsid(CommonUtility::commString2Str(navigationPaneClsid));

    // CommString listingCursor;
    // CommonUtility::readValueFromStruct(dstruct, syncInfoListingCursor, listingCursor);
    // int64_t listingCursorTimestamp = 0;
    // CommonUtility::readValueFromStruct(dstruct, syncInfoListingCursorTimestamp, listingCursorTimestamp);
    // setListingCursor(CommonUtility::commString2Str(listingCursor), listingCursorTimestamp);
}

} // namespace KDC
