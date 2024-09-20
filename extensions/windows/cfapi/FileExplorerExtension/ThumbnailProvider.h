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

#include <thumbcache.h>
#include <winrt\base.h>

class __declspec(uuid("6C7A1B02-8DB8-495F-B9B0-C4AB26B6284D")) ThumbnailProvider
    : public winrt::implements<ThumbnailProvider, IInitializeWithItem, IThumbnailProvider> {
    public:
        ThumbnailProvider() = default;

        // IInitializeWithItem
        IFACEMETHODIMP Initialize(_In_ IShellItem *item, _In_ DWORD mode);

        // IThumbnailProvider
        IFACEMETHODIMP GetThumbnail(_In_ UINT width, _Out_ HBITMAP *bitmap, _Out_ WTS_ALPHATYPE *alphaType);

    private:
        winrt::com_ptr<IShellItem2> _item;

        bool getOffline(const std::wstring &path);
        std::wstring mimeTypeFromString(const std::wstring &ext);
};
