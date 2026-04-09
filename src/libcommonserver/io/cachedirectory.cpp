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

#include "cachedirectory.h"

#include "config.h" // APPLICATION

#include <format>

namespace KDC {

CacheDirectory::CacheDirectory(const SyncPath &localSyncPath) {
    initDirectory(localSyncPath);
}

CacheDirectory::~CacheDirectory() {
    deleteDirectory();
}

const SyncPath &CacheDirectory::path() noexcept {
    const std::scoped_lock lock(_mutex);

    bool exists = false;
    if (auto ioError = IoError::Unknown;
        !IoHelper::checkIfPathExists(_cacheDirectoryPath, exists, ioError, IoHelper::PathCheckOption::Insensitive)) {
        sentry::Handler::captureMessage(sentry::Level::Error, "Failed to check if kDrive-cache exist",
                                        CommonUtility::ws2s(Utility::formatIoError(_cacheDirectoryPath, ioError)));
    }

    if (!exists) {
        resetDirectoryPath();
    }
    return _cacheDirectoryPath;
}

void CacheDirectory::initDirectory(const SyncPath &localSyncPath) noexcept {
    const auto cacheDirName = std::format(".{}-cache", APPLICATION_NAME);
    _cacheDirectoryPath = localSyncPath / cacheDirName;

    if (auto ioError = IoError::Success;
        !IoHelper::createDirectory(_cacheDirectoryPath, false, ioError) && ioError != IoError::DirectoryExists) {
        sentry::Handler::captureMessage(sentry::Level::Error, "Failed to create kDrive-cache",
                                        CommonUtility::ws2s(Utility::formatIoError(_cacheDirectoryPath, ioError)));
        return;
    }

    return;
}

void CacheDirectory::deleteDirectory() const noexcept {
    // It is the best effort, we cannot log/sentry anything here as the logger/sentry may have been destroyed already.
    auto ioError = IoError::Success;
    (void) IoHelper::deleteItem(_cacheDirectoryPath, ioError);
}

void CacheDirectory::resetDirectoryPath() noexcept {
    deleteDirectory();
    initDirectory(_cacheDirectoryPath.parent_path());
}

} // namespace KDC
