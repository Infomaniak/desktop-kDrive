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

#pragma once

#include <QDataStream>

#include <Poco/Dynamic/Struct.h>

namespace KDC {

class PackInfo {
    public:
        PackInfo() = default;
        PackInfo(const uint64_t id, const std::string &name, const std::string &displayName, const bool isFree);

        [[nodiscard]] uint64_t id() const { return _id; }
        void setId(const uint64_t id) { _id = id; }
        [[nodiscard]] const std::string &name() const { return _name; }
        void setName(const std::string_view name) { _name = name; }
        [[nodiscard]] const std::string &displayName() const { return _displayName; }
        void setDisplayName(const std::string_view displayName) { _displayName = displayName; }
        [[nodiscard]] bool isFree() const { return _isFree; }
        void setIsFree(const bool isFree) { _isFree = isFree; }

        void toDynamicStruct(Poco::DynamicStruct &dstruct) const;
        void fromDynamicStruct(const Poco::DynamicStruct &dstruct);

        friend bool operator==(const PackInfo &lhs, const PackInfo &rhs) {
            return lhs.id() == rhs.id() && lhs.name() == rhs.name() && lhs.displayName() == rhs.displayName() &&
                   lhs.isFree() == rhs.isFree();
        }

    private:
        uint64_t _id{0};
        std::string _name;
        std::string _displayName;
        bool _isFree{false}; // False by default in order not to offer to upgrade when this value is not yet up to date.
};


} // namespace KDC
