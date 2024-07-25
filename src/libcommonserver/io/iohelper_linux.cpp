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

#include "libcommonserver/io/filestat.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/utility/utility.h"

#include <log4cplus/loggingmacros.h>

#include <sys/types.h>
#include <sys/stat.h>

namespace KDC {

bool IoHelper::checkIfFileIsDehydrated(const SyncPath &itemPath, bool &isDehydrated, IoError &ioError) noexcept {
    (void)(itemPath);

    isDehydrated = false;
    ioError = IoErrorSuccess;

    return true;
}

bool IoHelper::getRights(const SyncPath &path, bool &read, bool &write, bool &exec, IoError &ioError) noexcept {
    read = false;
    write = false;
    exec = false;

    std::error_code ec;
    std::filesystem::perms perms = std::filesystem::status(path, ec).permissions();
    if (ec) {
        const bool exists = (ec.value() != static_cast<int>(std::errc::no_such_file_or_directory));
        ioError = stdError2ioError(ec);
        if (!exists) {
            ioError = IoErrorNoSuchFileOrDirectory;
        }
        return isExpectedError(ioError);
    }

    read = ((perms & std::filesystem::perms::owner_read) != std::filesystem::perms::none);
    write = ((perms & std::filesystem::perms::owner_write) != std::filesystem::perms::none);
    exec = ((perms & std::filesystem::perms::owner_exec) != std::filesystem::perms::none);
    return true;
}

}  // namespace KDC
