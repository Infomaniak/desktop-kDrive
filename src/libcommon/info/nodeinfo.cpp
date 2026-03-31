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

#include "nodeinfo.h"
#include "utility/utility.h"

static const auto nodeInfoNodeId = "nodeId";
static const auto nodeInfoName = "name";
static const auto nodeInfoSize = "size";
static const auto nodeInfoParentNodeId = "parentNodeId";
static const auto nodeInfoModtime = "modtime";
static const auto nodeInfoPath = "path";
static const auto nodeInfoAccessDenied = "accessDenied";

namespace KDC {

NodeInfo::NodeInfo(QString nodeId, QString name, qint64 size, QString parentNodeId, SyncTime modtime, QString path /*= ""*/) :
    _nodeId(nodeId),
    _name(name),
    _size(size),
    _parentNodeId(parentNodeId),
    _modtime(modtime),
    _path(path) {}

NodeInfo::NodeInfo() :
    _size(-1) {}

void NodeInfo::toDynamicStruct(Poco::DynamicStruct &dstruct) const {
    CommonUtility::writeValueToStruct(dstruct, nodeInfoNodeId, CommonUtility::qStr2CommString(_nodeId));
    CommonUtility::writeValueToStruct(dstruct, nodeInfoName, CommonUtility::qStr2CommString(_name));
    CommonUtility::writeValueToStruct(dstruct, nodeInfoSize, _size);
    CommonUtility::writeValueToStruct(dstruct, nodeInfoParentNodeId, CommonUtility::qStr2CommString(_parentNodeId));
    CommonUtility::writeValueToStruct(dstruct, nodeInfoModtime, _modtime);
    CommonUtility::writeValueToStruct(dstruct, nodeInfoPath, CommonUtility::qStr2CommString(_path));
    CommonUtility::writeValueToStruct(dstruct, nodeInfoAccessDenied, _accessDenied);
}

void NodeInfo::fromDynamicStruct(const Poco::DynamicStruct &dstruct) {
    CommString nodeId;
    CommonUtility::readValueFromStruct(dstruct, nodeInfoNodeId, nodeId);
    _nodeId = CommonUtility::commString2QStr(nodeId);

    CommString name;
    CommonUtility::readValueFromStruct(dstruct, nodeInfoName, name);
    _name = CommonUtility::commString2QStr(name);

    CommonUtility::readValueFromStruct(dstruct, nodeInfoSize, _size);

    CommString parentNodeId;
    CommonUtility::readValueFromStruct(dstruct, nodeInfoParentNodeId, parentNodeId);
    _parentNodeId = CommonUtility::commString2QStr(parentNodeId);

    CommonUtility::readValueFromStruct(dstruct, nodeInfoModtime, _modtime);

    CommString path;
    CommonUtility::readValueFromStruct(dstruct, nodeInfoPath, path);
    _path = CommonUtility::commString2QStr(path);

    CommonUtility::readValueFromStruct(dstruct, nodeInfoAccessDenied, _accessDenied);
}

QDataStream &operator>>(QDataStream &in, NodeInfo &info) {
    in >> info._nodeId >> info._name >> info._size >> info._parentNodeId >> info._modtime >> info._path >> info._accessDenied;
    return in;
}

QDataStream &operator<<(QDataStream &out, const NodeInfo &info) {
    out << info._nodeId << info._name << info._size << info._parentNodeId << info._modtime << info._path << info._accessDenied;
    return out;
}

QDataStream &operator<<(QDataStream &out, const QList<NodeInfo> &list) {
    int count = static_cast<int>(list.size());
    out << count;
    for (int i = 0; i < count; i++) {
        NodeInfo info = list[i];
        out << info;
    }
    return out;
}

QDataStream &operator>>(QDataStream &in, QList<NodeInfo> &list) {
    auto count = 0;
    in >> count;
    for (int i = 0; i < count; i++) {
        NodeInfo info;
        in >> info;
        list.push_back(info);
    }
    return in;
}

} // namespace KDC
