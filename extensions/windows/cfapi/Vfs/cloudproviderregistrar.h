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
#include "providerinfo.h"

#include <sddl.h>
#include <winrt\base.h>

class CloudProviderRegistrar {
    public:
        static std::wstring registerWithShell(ProviderInfo *providerInfo, wchar_t *namespaceCLSID, DWORD *namespaceCLSIDSize);
        static bool unregister(std::wstring syncRootID);

    private:
        static std::unique_ptr<TOKEN_USER> getTokenInformation();
        static std::wstring getSyncRootId(const ProviderInfo *providerInfo);
        /*static void addCustomState(
            _In_ winrt::IVector<winrt::StorageProviderItemPropertyDefinition> &customStates,
            _In_ LPCWSTR displayNameResource,
            _In_ int id);*/

        static winrt::com_array<wchar_t> convertSidToStringSid(PSID sid);
};
