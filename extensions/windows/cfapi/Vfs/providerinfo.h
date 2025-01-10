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

#include "..\Common\framework.h"

#include <mutex>
#include <unordered_map>

#include <cfapi.h>

struct FetchInfo {
        inline void setUpdating(bool updating) { _updating = updating; }
        inline bool getUpdating() const { return _updating; }
        inline void setCancel() { _cancel = true; }
        inline bool getCancel() const { return _cancel; }

        CF_CONNECTION_KEY _connectionKey;
        CF_TRANSFER_KEY _transferKey;
        LARGE_INTEGER _start;
        LARGE_INTEGER _offset;
        LARGE_INTEGER _length;
        bool _updating;
        bool _cancel;
};

class ProviderInfo {
    public:
        std::unordered_map<std::wstring, FetchInfo> _fetchMap;
        std::mutex _fetchMapMutex;
        std::condition_variable _cancelFetchCV;

        ProviderInfo(const LPCWSTR id, const LPCWSTR driveId, const LPCWSTR folderId, const LPCWSTR userId,
                     const LPCWSTR folderName, const LPCWSTR folderPath);

        inline LPCWSTR id() const { return _id.data(); }
        inline LPCWSTR driveId() const { return _driveId.data(); }
        inline LPCWSTR folderId() const { return _folderId.data(); }
        inline LPCWSTR userId() const { return _userId.data(); }
        inline LPCWSTR folderName() const { return _folderName.data(); }
        inline LPCWSTR folderPath() const { return _folderPath.data(); }

    private:
        std::wstring _id;
        std::wstring _driveId;
        std::wstring _folderId;
        std::wstring _userId;
        std::wstring _folderName;
        std::wstring _folderPath;
};
