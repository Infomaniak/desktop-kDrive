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

#include "testhelpers.h"
#include "localtemporarydirectory.h"
#include "io/iohelper.h"

#include "libcommon/utility/utility.h"
#include "libcommonserver/io/iohelper.h"

#include <fstream>

#include <sys/stat.h>
#include <utime.h>

namespace KDC::testhelpers {


void eraseFromTrash(const KDC::SyncPath &relativePath) {
    std::error_code ec;
    (void) std::filesystem::remove_all(Utility::getTrashPath() / relativePath, ec);
}

bool isInTrash(const SyncPath &path) {
    if (std::error_code ec; !std::filesystem::exists(Utility::getTrashPath() / path, ec) || ec) {
        if (ec) {
            LOGW_WARN(Log::instance()->getLogger(), L"Error in std::filesystem::exists - " << Utility::formatStdError(ec));
        }
        return false;
    }

    return true;
}

} // namespace KDC::testhelpers
