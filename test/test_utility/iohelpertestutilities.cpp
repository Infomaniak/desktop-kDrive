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

#include "iohelpertestutilities.h"

namespace KDC {

void IoHelperTestUtilities::setRename(std::function<void(const SyncPath &, const SyncPath &, std::error_code &)> f) {
    _rename = f;
}
void IoHelperTestUtilities::setIsDirectoryFunction(std::function<bool(const SyncPath &path, std::error_code &ec)> f) {
    _isDirectory = f;
};
void IoHelperTestUtilities::setIsSymlinkFunction(std::function<bool(const SyncPath &path, std::error_code &ec)> f) {
    _isSymlink = f;
};
void IoHelperTestUtilities::setReadSymlinkFunction(std::function<SyncPath(const SyncPath &path, std::error_code &ec)> f) {
    _readSymlink = f;
};
void IoHelperTestUtilities::setFileSizeFunction(std::function<std::uintmax_t(const SyncPath &path, std::error_code &ec)> f) {
    _fileSize = f;
}

void IoHelperTestUtilities::setTempDirectoryPathFunction(std::function<SyncPath(std::error_code &ec)> f) {
    _tempDirectoryPath = f;
}

void IoHelperTestUtilities::setCacheDirectoryPath(const SyncPath &newPath) {
    IoHelper::setCacheDirectoryPath(newPath);
}

#if defined(KD_MACOS)
void IoHelperTestUtilities::setReadAliasFunction(
        std::function<bool(const SyncPath &path, SyncPath &targetPath, IoError &ioError)> f) {
    _readAlias = f;
};
#endif

void IoHelperTestUtilities::resetFunctions() {
    // Reset to default std::filesytem implementation.
    setRename(static_cast<void (*)(const SyncPath &srcPath, const SyncPath &destPath, std::error_code &ec)>(&std::filesystem::rename));
    setIsDirectoryFunction(static_cast<bool (*)(const SyncPath &path, std::error_code &ec)>(&std::filesystem::is_directory));
    setIsSymlinkFunction(static_cast<bool (*)(const SyncPath &path, std::error_code &ec)>(&std::filesystem::is_symlink));
    setReadSymlinkFunction(static_cast<SyncPath (*)(const SyncPath &path, std::error_code &ec)>(&std::filesystem::read_symlink));
    setFileSizeFunction(static_cast<std::uintmax_t (*)(const SyncPath &path, std::error_code &ec)>(&std::filesystem::file_size));
    setTempDirectoryPathFunction(static_cast<SyncPath (*)(std::error_code &ec)>(&std::filesystem::temp_directory_path));

#if defined(KD_MACOS)
    // Default Utility::readAlias implementation
    setReadAliasFunction([](const SyncPath &path, SyncPath &targetPath, IoError &ioError) -> bool {
        std::string data;
        return readAlias(path, data, targetPath, ioError);
    });
#endif
    IoHelper::setCacheDirectoryPath("");
}
} // namespace KDC
