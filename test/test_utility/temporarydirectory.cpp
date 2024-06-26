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
#include "temporarydirectory.h"

#include <sstream>

namespace KDC {

TemporaryDirectory::TemporaryDirectory(const std::string &testType) {
    const std::time_t now = std::time(nullptr);
    const std::tm tm = *std::localtime(&now);
    std::ostringstream woss;
    woss << std::put_time(&tm, "%Y%m%d_%H%M");

    path = std::filesystem::temp_directory_path() / ("kdrive_" + testType + "_unit_tests_" + woss.str());
    std::filesystem::create_directory(path);
}

TemporaryDirectory::~TemporaryDirectory() {
    std::filesystem::remove_all(path);
}


}  // namespace KDC
