/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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
#include <winrt/base.h>

class CloudProviderRegistrar {
    public:
        static std::wstring registerWithShell(ProviderInfo *providerInfo, wchar_t *namespaceCLSID, DWORD *namespaceCLSIDSize,
                                              int64_t *registeredAt);
        static bool unregister(std::wstring syncRootID);
        static bool isRegistered(const std::wstring &providerId, const std::wstring &userId, const wchar_t *folderPath,
                                 bool &registered);

    private:
        static std::unique_ptr<TOKEN_USER> getTokenInformation();
        static std::wstring getSyncRootId(const ProviderInfo *providerInfo);
        static std::wstring getSyncRootId(const std::wstring &providerId, const std::wstring &userId);
        static void getCLSID(HKEY hKey, wchar_t *namespaceCLSID, DWORD *namespaceCLSIDSize);
        static void updateAumidEntry(HKEY hKey);
        static void updateSyncRootRegistryEntries(const std::wstring &syncRootID, wchar_t *namespaceCLSID,
                                                  DWORD *namespaceCLSIDSize, bool updateIcons = false);

        // Returns true if the two folders are related (i.e. one is a subfolder of the other or they are the same folder)
        static bool areRelatedFolders(const std::wstring &folderPath1, const std::wstring &folderPath2);

        static void updateRegistration(const ProviderInfo *providerInfo);
        static bool createRegistration(const ProviderInfo *providerInfo, const std::wstring &syncRootID);
        /*static void addCustomState(
            _In_ winrt::IVector<winrt::StorageProviderItemPropertyDefinition> &customStates,
            _In_ LPCWSTR displayNameResource,
            _In_ int id);*/

        static winrt::com_array<wchar_t> convertSidToStringSid(PSID sid);
};
