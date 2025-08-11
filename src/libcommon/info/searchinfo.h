/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
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
#include "utility/types.h"

namespace KDC {

class SearchInfo {
    public:
        SearchInfo() = default;
        SearchInfo(const NodeId &id, const SyncName &name, const NodeType type) :
            _id(id),
            _name(name),
            _type(type) {}


        [[nodiscard]] const NodeId &id() const { return _id; }
        [[nodiscard]] const SyncName &name() const { return _name; }
        [[nodiscard]] NodeType type() const { return _type; }

        friend QDataStream &operator>>(QDataStream &in, SearchInfo &info) {
            QString tmpId;
            QString tmpName;
            int tmpType = 0;
            in >> tmpId >> tmpName >> tmpType;
            info._id = QStr2Str(tmpId);
            info._name = QStr2SyncName(tmpName);
            info._type = static_cast<NodeType>(tmpType);
            return in;
        }
        friend QDataStream &operator<<(QDataStream &out, const SearchInfo &info) {
            out << QString::fromStdString(info._id) << SyncName2QStr(info._name) << static_cast<int>(info._type);
            return out;
        }
        friend QDataStream &operator>>(QDataStream &in, QList<SearchInfo> &list) {
            auto count = 0;
            in >> count;
            for (auto i = 0; i < count; i++) {
                SearchInfo info;
                in >> info;
                list.push_back(info);
            }
            return in;
        }
        friend QDataStream &operator<<(QDataStream &out, const QList<SearchInfo> &list) {
            const auto count = list.size();
            out << count;
            for (auto i = 0; i < count; i++) {
                const SearchInfo &info = list[i];
                out << info;
            }
            return out;
        }

    private:
        NodeId _id;
        SyncName _name;
        NodeType _type{NodeType::Unknown};
};

} // namespace KDC
