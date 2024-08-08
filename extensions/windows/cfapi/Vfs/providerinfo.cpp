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

#include "providerinfo.h"

ProviderInfo::ProviderInfo(const LPCWSTR id, const LPCWSTR driveId, const LPCWSTR folderId, const LPCWSTR userId,
                           const LPCWSTR folderName, const LPCWSTR folderPath) {
    if (id) {
        _id = std::wstring(id);
    }

    if (driveId) {
        _driveId = std::wstring(driveId);
    }

    if (folderId) {
        _folderId = std::wstring(folderId);
    }

    if (userId) {
        _userId = std::wstring(userId);
    }

    if (folderName) {
        _folderName = std::wstring(folderName);
    }

    if (folderPath) {
        _folderPath = std::wstring(folderPath);
    }

    if (!_folderPath.empty()) {
        CreateDirectory(_folderPath.c_str(), nullptr);
    }
}
