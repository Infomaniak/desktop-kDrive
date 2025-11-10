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
#include "localtemporarydirectory.h"

#include "libcommonserver/io/filestat.h"
#include "libcommonserver/io/iohelper.h"

#include <sstream>

namespace KDC {

LocalTemporaryDirectory::LocalTemporaryDirectory(const std::string &testType, const SyncPath &destinationPath /*= {}*/) {
    const std::time_t now = std::time(nullptr);
    const std::tm tm = *std::localtime(&now);
    std::ostringstream woss;
    woss << std::put_time(&tm, "%Y%m%d_%H%M%S");

    const auto basePath = destinationPath.empty() ? std::filesystem::temp_directory_path() : destinationPath;
    _path = basePath / ("kdrive_" + testType + "_unit_tests_" + woss.str());
    int retryCount = 0;
    const int maxRetry = 100;
    while (!std::filesystem::create_directory(_path) && retryCount < maxRetry) {
        retryCount++;
        _path = basePath / ("kdrive_" + testType + "_unit_tests_" + woss.str() + "_" + std::to_string(retryCount));
    }

    if (retryCount == maxRetry) {
        throw std::runtime_error("Failed to create local temporary directory");
    }

    _path = std::filesystem::canonical(_path); // Follows symlinks to work around the symlink /var -> private/var on MacOSX.
    FileStat fileStat;
    IoError ioError = IoError::Success;
    IoHelper::getFileStat(_path, &fileStat, ioError);
    _id = std::to_string(fileStat.inode);
}

LocalTemporaryDirectory::~LocalTemporaryDirectory() {
    auto ioError = IoError::Success;
    (void) IoHelper::setRights(_path, true, true, true, ioError);

    std::error_code ec;
    std::filesystem::remove_all(_path, ec);
    if (ec) {
        // Cannot remove directory
        std::cout << ec.message() << std::endl;
        assert(false);
    }
}


} // namespace KDC
