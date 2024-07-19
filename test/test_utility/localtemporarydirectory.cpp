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
#include "localtemporarydirectory.h"

#include "io/filestat.h"
#include "io/iohelper.h"

#include <sstream>

namespace KDC {

LocalTemporaryDirectory::LocalTemporaryDirectory(const std::string &testType) {
    const std::time_t now = std::time(nullptr);
    const std::tm tm = *std::localtime(&now);
    std::ostringstream woss;
    woss << std::put_time(&tm, "%Y%m%d_%H%M");

    _path = std::filesystem::temp_directory_path() / ("kdrive_" + testType + "_unit_tests_" + woss.str());
    _path = std::filesystem::canonical(_path);  // Follows symlinks to work around the symlink /var -> private/var on MacOSX.
    std::filesystem::create_directory(_path);

    FileStat fileStat;
    IoError ioError = IoErrorSuccess;
    IoHelper::getFileStat(_path, &fileStat, ioError);
    _id = std::to_string(fileStat.inode);
}

LocalTemporaryDirectory::~LocalTemporaryDirectory() {
    std::filesystem::remove_all(_path);
}


}  // namespace KDC
