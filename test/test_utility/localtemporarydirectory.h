/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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

#include <string>
#include <filesystem>

namespace KDC {

class LocalTemporaryDirectory {
    public:
        explicit LocalTemporaryDirectory(const std::string& testType = "undef");
        ~LocalTemporaryDirectory();

        [[nodiscard]] const std::filesystem::path& path() const { return _path; }
        [[nodiscard]] const NodeId& id() const { return _id; }

    private:
        std::filesystem::path _path;
        NodeId _id;
};

}  // namespace KDC
