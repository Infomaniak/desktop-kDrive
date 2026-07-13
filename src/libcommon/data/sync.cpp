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
static const auto syncInfoTargetPath = "targetPath";
static const auto syncInfoTargetNodeId = "targetNodeId";
static const auto syncInfoSupportVfs = "supportVfs";
static const auto syncInfoVirtualFileMode = "virtualFileMode";
static const auto syncInfoNavigationPaneClsid = "navigationPaneClsid";

namespace KDC {

BaseSync::BaseSync(const SyncDbId dbId, const DriveDbId driveDbId, const std::filesystem::path &localPath,
                   const std::filesystem::path &targetPath, const NodeId &targetNodeId, const bool supportVfs,
                   const VirtualFileMode virtualFileMode, const std::string &navigationPaneClsid) :
    _dbId(dbId),
    _driveDbId(driveDbId),
    _localPath(localPath),
    _targetPath(targetPath),
    _targetNodeId(targetNodeId),
    _supportVfs(supportVfs),
    _virtualFileMode(virtualFileMode),
    _navigationPaneClsid(navigationPaneClsid) {}

void BaseSync::toDynamicStruct(Poco::DynamicStruct &dstruct) const {
    CommonUtility::writeValueToStruct(dstruct, syncInfoDbId, dbId());
    CommonUtility::writeValueToStruct(dstruct, syncInfoDriveDbId, driveDbId());
    CommonUtility::writeValueToStruct(dstruct, syncInfoLocalPath, CommonUtility::syncPath2CommString(localPath()));
    CommonUtility::writeValueToStruct(dstruct, syncInfoTargetPath, CommonUtility::syncPath2CommString(targetPath()));
    CommonUtility::writeValueToStruct(dstruct, syncInfoTargetNodeId, CommonUtility::str2CommString(targetNodeId()));
    CommonUtility::writeValueToStruct(dstruct, syncInfoSupportVfs, supportVfs());
    CommonUtility::writeValueToStruct(dstruct, syncInfoVirtualFileMode, virtualFileMode());
    CommonUtility::writeValueToStruct(dstruct, syncInfoNavigationPaneClsid, CommonUtility::str2CommString(navigationPaneClsid()));
}

void BaseSync::fromDynamicStruct(const Poco::DynamicStruct &dstruct) {
    SyncDbId syncDbId = 0;
    CommonUtility::readValueFromStruct(dstruct, syncInfoDbId, syncDbId);
    setDbId(syncDbId);

    DriveDbId driveDbId = 0;
    CommonUtility::readValueFromStruct(dstruct, syncInfoDriveDbId, driveDbId);
    setDriveDbId(driveDbId);

    CommString localPath;
    CommonUtility::readValueFromStruct(dstruct, syncInfoLocalPath, localPath);
    setLocalPath(CommonUtility::commString2SyncPath(localPath));

    CommString targetPath;
    CommonUtility::readValueFromStruct(dstruct, syncInfoTargetPath, targetPath);
    setTargetPath(CommonUtility::commString2SyncPath(targetPath));

    CommString targetNodeId;
    CommonUtility::readValueFromStruct(dstruct, syncInfoTargetNodeId, targetNodeId);
    setTargetNodeId(CommonUtility::commString2Str(targetNodeId));

    CommonUtility::readValueFromStruct(dstruct, syncInfoSupportVfs, _supportVfs);
    CommonUtility::readValueFromStruct(dstruct, syncInfoVirtualFileMode, _virtualFileMode);

    CommString navigationPaneClsid;
    CommonUtility::readValueFromStruct(dstruct, syncInfoNavigationPaneClsid, navigationPaneClsid);
    setNavigationPaneClsid(CommonUtility::commString2Str(navigationPaneClsid));
}

Sync::Sync(SyncDbId dbId, DriveDbId driveDbId, const std::filesystem::path &localPath, const NodeId &localNodeId,
           const std::filesystem::path &targetPath, const NodeId &targetNodeId, bool paused, bool supportVfs,
           VirtualFileMode virtualFileMode, bool notificationsDisabled, const std::filesystem::path &dbPath,
           bool hasFullyCompleted, const std::string &navigationPaneClsid, const std::string &listingCursor,
           int64_t listingCursorTimestamp) :
    BaseSync(dbId, driveDbId, localPath, targetPath, targetNodeId, supportVfs, virtualFileMode, navigationPaneClsid),
    _localNodeId(localNodeId),
    _paused(paused),
    _notificationsDisabled(notificationsDisabled),
    _dbPath(dbPath),
    _hasFullyCompleted(hasFullyCompleted),
    _listingCursor(listingCursor),
    _listingCursorTimestamp(listingCursorTimestamp) {}

} // namespace KDC
