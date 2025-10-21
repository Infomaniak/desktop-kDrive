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
#include "libcommon/utility/utility.h"

static const auto searchInfoId = "id";
static const auto searchInfoName = "name";
static const auto searchInfoType = "type";

namespace KDC {

SearchInfo::SearchInfo(const NodeId &id, const SyncName &name, const NodeType type) :
    _id(id),
    _name(name),
    _type(type) {}

void SearchInfo::toDynamicStruct(Poco::DynamicStruct &dstruct) const {
    CommonUtility::writeValueToStruct(dstruct, searchInfoId, _id);
    CommonUtility::writeValueToStruct(dstruct, searchInfoName, _name);
    CommonUtility::writeValueToStruct(dstruct, searchInfoType, _type);
}

void SearchInfo::fromDynamicStruct(const Poco::DynamicStruct &dstruct) {
    CommonUtility::readValueFromStruct(dstruct, searchInfoId, _id);
    CommonUtility::readValueFromStruct(dstruct, searchInfoName, _name);
    CommonUtility::readValueFromStruct(dstruct, searchInfoType, _type);
}

QDataStream &operator>>(QDataStream &in, SearchInfo &info) {
    QString tmpId;
    QString tmpName;
    int tmpType = 0;
    in >> tmpId >> tmpName >> tmpType;
    info._id = QStr2Str(tmpId);
    info._name = QStr2SyncName(tmpName);
    info._type = static_cast<NodeType>(tmpType);
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
    out << QString::fromStdString(info._id) << SyncName2QStr(info._name) << static_cast<int>(info._type);
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
