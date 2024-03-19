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

#pragma once

#include "..\Common\framework.h"

#include <cfapi.h>
#include <winrt\windows.storage.provider.h>

namespace winrt {
using namespace Windows::Storage::Provider;
}

class Placeholders {
    public:
        static bool create(const PCWSTR fileId, const PCWSTR relativePath, const PCWSTR destPath,
                           const WIN32_FIND_DATA *findData);

        static bool convert(const PCWSTR fileId, const PCWSTR filePath);

        static bool revert(const PCWSTR filePath);

        static bool update(const PCWSTR filePath, const WIN32_FIND_DATA *findData);

        static bool getStatus(const PCWSTR filePath, bool *isPlaceholder, bool *isDehydrated, bool *isSynced);

        static bool setStatus(const PCWSTR path, bool syncOngoing);

        static bool getInfo(const PCWSTR path, CF_PLACEHOLDER_STANDARD_INFO &info);

        static bool setPinState(const PCWSTR path, CF_PIN_STATE state);
};
