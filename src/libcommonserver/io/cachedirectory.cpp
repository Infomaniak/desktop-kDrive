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

#include <regex>

namespace KDC {

CacheDirectory::CacheDirectory(const SyncPath &localSyncPath) :
    _syncDirectoryPath(localSyncPath) {}

CacheDirectory::~CacheDirectory() {
    cleanUp();
}

ExitInfo CacheDirectory::path(SyncPath &cacheDirectory) noexcept {
    const std::scoped_lock lock(_mutex);

    bool exists = false;
    if (auto ioError = IoError::Unknown;
        !IoHelper::checkIfPathExists(_cacheDirectoryPath, exists, ioError, IoHelper::PathCheckOption::Insensitive)) {
        sentry::Handler::captureMessage(sentry::Level::Error, "Failed to check if kDrive-cache exist",
                                        CommonUtility::ws2s(Utility::formatIoError(_cacheDirectoryPath, ioError)));
    }
    if (!exists) {
        if (const auto exitInfo = initDirectory(); !exitInfo) return exitInfo;
    }

    cacheDirectory = _cacheDirectoryPath;
    return ExitCode::Ok;
}

ExitInfo CacheDirectory::initDirectory() noexcept {
    if (_syncDirectoryPath.empty()) {
        return {ExitCode::LogicError, ExitCause::InvalidArgument};
    }

    const auto cacheDirName = Poco::format(".%s-cache", std::string(APPLICATION_NAME));
    _cacheDirectoryPath = _syncDirectoryPath / cacheDirName;

    if (auto ioError = IoError::Success;
        !IoHelper::createDirectory(_cacheDirectoryPath, false, ioError) && ioError != IoError::DirectoryExists) {
        sentry::Handler::captureMessage(sentry::Level::Error, "Failed to create kDrive-cache",
                                        CommonUtility::ws2s(Utility::formatIoError(_cacheDirectoryPath, ioError)));
        return {ExitCode::SystemError, ExitCause::TmpDirAccessError};
    }

    // Hide cache directory
    IoHelper::setFileHidden(_cacheDirectoryPath, true);

    return ExitCode::Ok;
}

void CacheDirectory::cleanUp() const {
    auto ioError = IoError::Unknown;
    IoHelper::DirectoryIterator dirIt(_cacheDirectoryPath, false, ioError);
    bool endOfDir = false;
    DirectoryEntry entry;
    while (dirIt.next(entry, endOfDir, ioError) && !endOfDir) {
        if (std::regex_match(SyncName2Str(entry.path().filename().native()), std::regex(R"(kdrive_.{10})"))) {
            (void) IoHelper::deleteItem(entry.path());
        }
    }
}

} // namespace KDC
