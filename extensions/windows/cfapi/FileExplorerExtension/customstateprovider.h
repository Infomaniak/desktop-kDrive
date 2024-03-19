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

#include "CustomStateProvider.g.h"
#include <windows.storage.provider.h>

// D19AA847-DAAE-45C0-9F53-702179617F42
constexpr CLSID CLSID_CustomStateProvider = {0xd19aa847, 0xdaae, 0x45c0, {0x9f, 0x53, 0x70, 0x21, 0x79, 0x61, 0x7f, 0x42}};

namespace winrt::FileExplorerExtension::implementation {
struct CustomStateProvider : CustomStateProviderT<CustomStateProvider> {
        CustomStateProvider() = default;

        Windows::Foundation::Collections::IIterable<Windows::Storage::Provider::StorageProviderItemProperty> GetItemProperties(
            _In_ hstring const& itemPath);
};
}  // namespace winrt::FileExplorerExtension::implementation

namespace winrt::FileExplorerExtension::factory_implementation {
struct CustomStateProvider : CustomStateProviderT<CustomStateProvider, implementation::CustomStateProvider> {};
}  // namespace winrt::FileExplorerExtension::factory_implementation
