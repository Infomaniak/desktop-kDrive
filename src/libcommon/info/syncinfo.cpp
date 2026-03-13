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

#include "syncinfo.h"
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

SyncInfo::SyncInfo(SyncDbId dbId, DriveDbId driveDbId, const QString &localPath, const QString &targetPath,
                   const QString &targetNodeId, bool supportVfs, VirtualFileMode virtualFileMode,
                   const QString &navigationPaneClsid) :
    _dbId(dbId),
    _driveDbId(driveDbId),
    _localPath(localPath),
    _targetPath(targetPath),
    _targetNodeId(targetNodeId),
    _supportVfs(supportVfs),
    _virtualFileMode(virtualFileMode),
    _navigationPaneClsid(navigationPaneClsid) {}

void SyncInfo::toDynamicStruct(Poco::DynamicStruct &dstruct) const {
    CommonUtility::writeValueToStruct(dstruct, syncInfoDbId, _dbId);
    CommonUtility::writeValueToStruct(dstruct, syncInfoDriveDbId, _driveDbId);
    CommonUtility::writeValueToStruct(dstruct, syncInfoLocalPath, CommonUtility::qStr2CommString(_localPath));
    CommonUtility::writeValueToStruct(dstruct, syncInfoTargetPath, CommonUtility::qStr2CommString(_targetPath));
    CommonUtility::writeValueToStruct(dstruct, syncInfoTargetNodeId, CommonUtility::qStr2CommString(_targetNodeId));
    CommonUtility::writeValueToStruct(dstruct, syncInfoSupportVfs, _supportVfs);
    CommonUtility::writeValueToStruct(dstruct, syncInfoVirtualFileMode, _virtualFileMode);
    CommonUtility::writeValueToStruct(dstruct, syncInfoNavigationPaneClsid, CommonUtility::qStr2CommString(_navigationPaneClsid));
}

void SyncInfo::fromDynamicStruct(const Poco::DynamicStruct &dstruct) {
    CommonUtility::readValueFromStruct(dstruct, syncInfoDbId, _dbId);
    CommonUtility::readValueFromStruct(dstruct, syncInfoDriveDbId, _driveDbId);

    CommString localPath;
    CommonUtility::readValueFromStruct(dstruct, syncInfoLocalPath, localPath);
    _localPath = CommonUtility::commString2QStr(localPath);

    CommString targetPath;
    CommonUtility::readValueFromStruct(dstruct, syncInfoTargetPath, targetPath);
    _targetPath = CommonUtility::commString2QStr(targetPath);

    CommString targetNodeId;
    CommonUtility::readValueFromStruct(dstruct, syncInfoTargetNodeId, targetNodeId);
    _targetNodeId = CommonUtility::commString2QStr(targetNodeId);

    CommonUtility::readValueFromStruct(dstruct, syncInfoSupportVfs, _supportVfs);
    CommonUtility::readValueFromStruct(dstruct, syncInfoVirtualFileMode, _virtualFileMode);

    CommString navigationPaneClsid;
    CommonUtility::readValueFromStruct(dstruct, syncInfoNavigationPaneClsid, navigationPaneClsid);
    _navigationPaneClsid = CommonUtility::commString2QStr(navigationPaneClsid);
}

QDataStream &operator>>(QDataStream &in, SyncInfo &info) {
    int dbId = 0; // must be int to work with QDataStreamd
    int driveDbId = 0; // must be int to work with QDataStreamd
    in >> dbId >> driveDbId >> info._localPath >> info._targetPath >> info._targetNodeId >> info._supportVfs >>
            info._virtualFileMode >> info._navigationPaneClsid;
    info.setDbId(dbId);
    info.setDriveDbId(driveDbId);
    return in;
}

QDataStream &operator<<(QDataStream &out, const SyncInfo &info) {
    out << toInt(info._dbId) << toInt(info._driveDbId) << info._localPath << info._targetPath << info._targetNodeId
        << info._supportVfs << info._virtualFileMode << info._navigationPaneClsid;
    return out;
}

QDataStream &operator<<(QDataStream &out, const QList<SyncInfo> &list) {
    int count = static_cast<int>(list.size());
    out << count;
    for (int i = 0; i < count; i++) {
        SyncInfo info = list[i];
        out << info;
    }
    return out;
}

QDataStream &operator>>(QDataStream &in, QList<SyncInfo> &list) {
    int count = 0;
    in >> count;
    for (int i = 0; i < count; i++) {
        SyncInfo info;
        in >> info;
        list.push_back(info);
    }
    return in;
}

} // namespace KDC
