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

#include "searchinfo.h"
#include "utility/utility.h"

static const auto searchInfoId = "id";
static const auto searchInfoName = "name";
static const auto searchInfoType = "type";
static const auto searchInfoPath = "path";
static const auto searchInfoModifiedTime = "modifiedTime";
static const auto searchInfoSize = "size";
static const auto searchInfoIsAvailableLocally = "isAvailableLocally";

namespace KDC {

SearchInfo::SearchInfo(const NodeId &id, const SyncName &name, const NodeType type, const SyncPath &path,
                       const SyncTime modifiedTime, const size_t size, const bool isAvailableLocally) :
    _id(id),
    _name(name),
    _path(path),
    _modifiedTime(modifiedTime),
    _size(static_cast<int64_t>(size)),
    _isAvailableLocally(isAvailableLocally),
    _type(type) {}

void SearchInfo::toDynamicStruct(Poco::DynamicStruct &dstruct) const {
    CommonUtility::writeValueToStruct(dstruct, searchInfoId, _id);
    CommonUtility::writeValueToStruct(dstruct, searchInfoName, _name);
    CommonUtility::writeValueToStruct(dstruct, searchInfoType, _type);
    CommonUtility::writeValueToStruct(dstruct, searchInfoPath, Path2Str(_path));
    CommonUtility::writeValueToStruct(dstruct, searchInfoModifiedTime, _modifiedTime);
    CommonUtility::writeValueToStruct(dstruct, searchInfoSize, _size);
    CommonUtility::writeValueToStruct(dstruct, searchInfoIsAvailableLocally, _isAvailableLocally);
}

void SearchInfo::fromDynamicStruct(const Poco::DynamicStruct &dstruct) {
    CommonUtility::readValueFromStruct(dstruct, searchInfoId, _id);
    CommonUtility::readValueFromStruct(dstruct, searchInfoName, _name);
    CommonUtility::readValueFromStruct(dstruct, searchInfoType, _type);
    std::string pathStr;
    CommonUtility::readValueFromStruct(dstruct, searchInfoPath, pathStr);
    _path = Str2Path(pathStr);
    CommonUtility::readValueFromStruct(dstruct, searchInfoModifiedTime, _modifiedTime);
    CommonUtility::readValueFromStruct(dstruct, searchInfoSize, _size);
    CommonUtility::readValueFromStruct(dstruct, searchInfoIsAvailableLocally, _isAvailableLocally);
}

QDataStream &operator>>(QDataStream &in, SearchInfo &info) {
    QString tmpId;
    QString tmpName;
    int tmpType = 0;
    QString tmpPath;
    qint64 tmpModifiedTime = 0;
    qint64 tmpSize = 0;
    bool tmpIsAvailableLocally = false;

    in >> tmpId >> tmpName >> tmpType >> tmpPath >> tmpModifiedTime >> tmpSize >> tmpIsAvailableLocally;
    info._id = QStr2Str(tmpId);
    info._name = QStr2SyncName(tmpName);
    info._type = static_cast<NodeType>(tmpType);
    info._path = QStr2Path(tmpPath);
    info._modifiedTime = static_cast<SyncTime>(tmpModifiedTime);
    info._size = static_cast<int64_t>(tmpSize);
    info._isAvailableLocally = tmpIsAvailableLocally;
    return in;
}

QDataStream &operator<<(QDataStream &out, const QList<SearchInfo> &list) {
    const auto count = list.size();
    out << count;
    for (auto i = 0; i < count; i++) {
        const SearchInfo &info = list[i];
        out << info;
    }
    return out;
}

QDataStream &operator<<(QDataStream &out, const SearchInfo &info) {
    out << QString::fromStdString(info._id) << SyncName2QStr(info._name) << static_cast<int>(info._type) << Path2QStr(info._path)
        << static_cast<qint64>(info._modifiedTime) << static_cast<qint64>(info._size) << info._isAvailableLocally;
    return out;
}

QDataStream &operator>>(QDataStream &in, QList<SearchInfo> &list) {
    auto count = 0;
    in >> count;
    for (auto i = 0; i < count; i++) {
        SearchInfo info;
        in >> info;
        list.push_back(info);
    }
    return in;
}


} // namespace KDC
