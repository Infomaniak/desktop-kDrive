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

#include "libcommon/utility/types.h"

namespace KDC {

struct FileStat {
        SyncTime creationTime = 0;
        SyncTime modificationTime = 0;
        int64_t size = 0;
        uint64_t inode = 0;
        bool isHidden = false;
        // Type of the item or target item if symlink
        // Value for a dangling symlink: NodeType::Unknown (macOS & Linux), NodeType::File/NodeType::Directory (Windows)
        NodeType nodeType = NodeType::Unknown;
        uint32_t _flags{0};
};

} // namespace KDC
