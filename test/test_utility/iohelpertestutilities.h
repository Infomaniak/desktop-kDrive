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
#include "libcommonserver/io/iohelper.h"
namespace KDC {
struct IoHelperTestUtilities : public IoHelper {
        static void setRename(std::function<void(const SyncPath &, const SyncPath &, std::error_code &)> f);
        static void setIsDirectoryFunction(std::function<bool(const SyncPath &path, std::error_code &ec)> f);
        static void setIsSymlinkFunction(std::function<bool(const SyncPath &path, std::error_code &ec)> f);
        static void setReadSymlinkFunction(std::function<SyncPath(const SyncPath &path, std::error_code &ec)> f);
        static void setFileSizeFunction(std::function<std::uintmax_t(const SyncPath &path, std::error_code &ec)> f);
        static void setTempDirectoryPathFunction(std::function<SyncPath(std::error_code &ec)> f);
        static void setCacheDirectoryPath(const SyncPath &newPath);

#ifdef __APPLE__
        static void setReadAliasFunction(std::function<bool(const SyncPath &path, SyncPath &targetPath, IoError &ioError)> f);
#endif

        static void resetFunctions();
};
} // namespace KDC
