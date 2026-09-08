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

#include "utility/types.h"

#include <Poco/Dynamic/Struct.h>

namespace KDC {

class NodeConflictInfo {
    public:
        NodeConflictInfo() = default;
        NodeConflictInfo(const std::string &authorName, const int64_t fileSize, const SyncTime lastModificationDate);

        [[nodiscard]] std::string_view authorName() const { return _authorName; }
        void setAuthorName(const std::string_view authorName) { _authorName = authorName; }

        [[nodiscard]] int64_t fileSize() const { return _fileSize; }
        void setFileSize(const int64_t fileSize) { _fileSize = fileSize; }
        [[nodiscard]] SyncTime lastModificationDate() const { return _lastModificationDate; }
        void setLastModificationDate(const SyncTime lastModificationDate) { _lastModificationDate = lastModificationDate; }

        void toDynamicStruct(Poco::DynamicStruct &dstruct) const;
        void fromDynamicStruct(const Poco::DynamicStruct &dstruct);

    private:
        std::string _authorName;
        int64_t _fileSize{0};
        SyncTime _lastModificationDate{0};
};

} // namespace KDC
