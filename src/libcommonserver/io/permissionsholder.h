/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
 ** This program is free software: you can redistribute it and/or modify
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

#include "iohelper.h"
#include "utility/types.h"

namespace KDC {

static std::unordered_map<SyncPath, uint64_t> counts;
static std::mutex mutex;

class PermissionsHolder {
    public:
        PermissionsHolder(const SyncPath &path) :
            _path(path) {
            bool read = false;
            bool write = false;
            bool exec = false;
            if (IoError ioError = IoHelper::getRights(path, read, write, exec); ioError != IoError::Success) {
                LOGW_DEBUG(Log::instance()->getLogger(), L"Fail to get rights for: " << Utility::formatSyncPath(path));
                return;
            }
            if (write) {
                // TODO : for now, only items in read only mode are supported.
                return;
            }

            std::scoped_lock lock(mutex);
            IoHelper::unsetReadOnly(_path);
            counts.try_emplace(_path, 0);
            counts[_path]++;
            LOGW_DEBUG(Log::instance()->getLogger(),
                       L"PermissionsHolder unsetReadOnly: " << Utility::formatSyncPath(_path) << L" / " << counts[_path]);
        }
        ~PermissionsHolder() {
            std::scoped_lock lock(mutex);
            if (!counts.contains(_path)) return;

            counts[_path]--;
            LOGW_DEBUG(Log::instance()->getLogger(),
                       L"PermissionsHolder value: " << Utility::formatSyncPath(_path) << L" / " << counts[_path]);
            if (counts[_path] > 0) return; // Do not put back read only rights yet.

            LOGW_DEBUG(Log::instance()->getLogger(),
                       L"PermissionsHolder setReadOnly: " << Utility::formatSyncPath(_path) << L" / " << counts[_path]);
            counts.erase(_path);
            IoHelper::setReadOnly(_path);
        }

    private:
        SyncPath _path;
};

} // namespace KDC
