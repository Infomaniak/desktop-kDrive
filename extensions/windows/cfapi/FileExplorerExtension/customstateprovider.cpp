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

#include "..\Common\utilities.h"
#include "customstateprovider.h"

namespace winrt {
using namespace winrt::Windows::Storage::Provider;
}

namespace winrt::FileExplorerExtension::implementation {
Windows::Foundation::Collections::IIterable<Windows::Storage::Provider::StorageProviderItemProperty>
CustomStateProvider::GetItemProperties(_In_ hstring const& itemPath) {
    // TRACE_DEBUG(L"CustomStateProvider::GetItemProperties");

    std::hash<std::wstring> hashFunc;
    auto hash = hashFunc(itemPath.c_str());

    auto propertyVector{winrt::single_threaded_vector<winrt::StorageProviderItemProperty>()};

    /*if ((hash & 0x1) != 0)
    {
        winrt::StorageProviderItemProperty itemProperty;
        itemProperty.Id(2);
        itemProperty.Value(L"Value2");
        itemProperty.IconResource(L"shell32.dll,-14");
        propertyVector.Append(itemProperty);
    }*/

    return propertyVector;
}
} // namespace winrt::FileExplorerExtension::implementation
