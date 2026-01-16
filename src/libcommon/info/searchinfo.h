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

#include <Poco/Dynamic/Struct.h>

namespace KDC {

class SearchInfo {
    public:
        SearchInfo() = default;
        SearchInfo(const NodeId &id, const SyncName &name, const NodeType type, const SyncPath &path, const SyncTime modifiedTime,
                   const size_t size, const bool isAvailableLocally);

        [[nodiscard]] const NodeId &id() const { return _id; }
        [[nodiscard]] const SyncName &name() const { return _name; }
        [[nodiscard]] NodeType type() const { return _type; }
        [[nodiscard]] const SyncPath &path() const { return _path; }
        [[nodiscard]] SyncTime lastModifiedTime() const { return _modifiedTime; }
        [[nodiscard]] int64_t size() const { return _size; }
        [[nodiscard]] bool isAvailableLocally() const { return _isAvailableLocally; }

        void toDynamicStruct(Poco::DynamicStruct &dstruct) const;
        void fromDynamicStruct(const Poco::DynamicStruct &dstruct);

        friend QDataStream &operator>>(QDataStream &in, SearchInfo &info);
        friend QDataStream &operator<<(QDataStream &out, const SearchInfo &info);
        friend QDataStream &operator>>(QDataStream &in, QList<SearchInfo> &list);
        friend QDataStream &operator<<(QDataStream &out, const QList<SearchInfo> &list);

    private:
        NodeId _id;
        SyncName _name;
        SyncPath _path;
        SyncTime _modifiedTime{0};
        int64_t _size{0};
        bool _isAvailableLocally{false};
        NodeType _type{NodeType::Unknown};
};

} // namespace KDC
