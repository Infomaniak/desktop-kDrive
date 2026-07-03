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

#include "cloudproviderregistrar.h"
#include "..\Common\utilities.h"

#include <winrt\windows.storage.provider.h>
#include <winrt\windows.security.cryptography.h>
namespace winrt {
using namespace Windows::Foundation;
using namespace Windows::Storage;
using namespace Windows::Storage::Streams;
using namespace Windows::Storage::Provider;
using namespace Windows::Foundation::Collections;
using namespace Windows::Security::Cryptography;
} // namespace winrt

#define REGPATH_SYNCROOTMANAGER L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\SyncRootManager\\"

#define REGPATH_HKEY_CLASSES_ROOT_CLSID L"CLSID\\"
#define REGPATH_HKEY_CLASSES_ROOT_WOW6432_CLSID L"WOW6432Node\\CLSID\\"
#define REGPATH_HKEY_CURRENT_USER_CLSID L"Software\\Classes\\CLSID\\"
#define REGPATH_HKEY_CURRENT_USER_WOW6432_CLSID L"Software\\Classes\\WOW6432Node\\CLSID\\"
#define REGKEY_NAMESPACECLSID L"NamespaceCLSID"
#define REGKEY_AUMID L"AUMID"
#define REGKEY_USERSYNCROOTS L"UserSyncRoots"
#define REGKEY_ICONRESOURCE L"IconResource"
#define REGKEY_DEFAULTICON L"DefaultIcon"

void updateRegistryEntry(const HKEY &hKey, const std::wstring &name, const std::wstring &value) {
    TRACE_INFO(L"%s value: %s", name.c_str(), value.c_str());

    if (RegSetValueEx(hKey, name.c_str(), 0, REG_SZ, (BYTE *) value.c_str(), (DWORD) (value.size() + 1) * sizeof(wchar_t)) !=
        ERROR_SUCCESS) {
        TRACE_ERROR(L"Could not set registry value %s=%s", name.c_str(), value.c_str());
    }
}

void CloudProviderRegistrar::updateRegistrationWithShell(std::wstring &syncRootID, wchar_t *namespaceCLSID,
                                                         DWORD *namespaceCLSIDSize) {
    HKEY hKey;
    std::wstring subKey = REGPATH_SYNCROOTMANAGER + syncRootID;
    TRACE_DEBUG(L"Provider already registered, opening key %s", subKey.c_str());
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, subKey.c_str(), 0, KEY_ALL_ACCESS, &hKey) != ERROR_SUCCESS) {
        TRACE_ERROR(L"Could not open key %s", subKey.c_str());
        return;
    }

    TRACE_DEBUG(L"Opened key %s", subKey.c_str());

    if (namespaceCLSID) {
        // Get CLSID
        TRACE_DEBUG(L"Getting NamespaceCLSID value");
        if (RegGetValue(hKey, 0, REGKEY_NAMESPACECLSID, RRF_RT_ANY, nullptr, namespaceCLSID, namespaceCLSIDSize) !=
            ERROR_SUCCESS) {
            TRACE_ERROR(L"Could not get registry value NamespaceCLSID");
        }
    }

    TRACE_DEBUG(L"Setting registry values");
    // Set default key
    if (RegSetValueEx(hKey, nullptr, 0, REG_SZ, (BYTE *) Utilities::s_appName.c_str(),
                      (DWORD) (Utilities::s_appName.size() + 1) * sizeof(wchar_t)) != ERROR_SUCCESS) {
        TRACE_ERROR(L"Could not set default registry value");
    }

    // Update AMUID key
    std::wstring name(REGKEY_AUMID);
    const std::wstring aumidValue = KDC_AUMID;
    std::wstring value = L"Infomaniak.kDrive.Extension_" + aumidValue + L"!App";
    updateRegistryEntry(hKey, name, value);

    // Update IconResource
    name = REGKEY_ICONRESOURCE;
    WCHAR exePath[MAX_FULL_PATH];
    if (!GetModuleFileNameW(nullptr, exePath, MAX_FULL_PATH)) {
        TRACE_ERROR(L"Error in GetModuleFileNameW");
    }
    value = exePath;
    if (!value.empty()) {
        updateRegistryEntry(hKey, name, value);
    }

    TRACE_DEBUG(L"Closing key %s", subKey.c_str());
    if (RegCloseKey(hKey) != ERROR_SUCCESS) {
        TRACE_ERROR(L"Could not close key %s", subKey.c_str());
    }

    if (!namespaceCLSID) return;

    // Update DefaultIcon keys
    struct RegKeyInfo {
            HKEY rootKey;
            std::wstring subKey;
    };
    std::vector<RegKeyInfo> regKeys = {
            {HKEY_CLASSES_ROOT,
             REGPATH_HKEY_CLASSES_ROOT_CLSID + std::wstring(namespaceCLSID) + L"\\" + std::wstring(REGKEY_DEFAULTICON)},
            {HKEY_CLASSES_ROOT,
             REGPATH_HKEY_CLASSES_ROOT_WOW6432_CLSID + std::wstring(namespaceCLSID) + L"\\" + std::wstring(REGKEY_DEFAULTICON)},
            {HKEY_CURRENT_USER,
             REGPATH_HKEY_CURRENT_USER_CLSID + std::wstring(namespaceCLSID) + L"\\" + std::wstring(REGKEY_DEFAULTICON)},
            {HKEY_CURRENT_USER,
             REGPATH_HKEY_CURRENT_USER_WOW6432_CLSID + std::wstring(namespaceCLSID) + L"\\" + std::wstring(REGKEY_DEFAULTICON)}};

    for (const auto &regKeyInfo: regKeys) {
        if (RegOpenKeyEx(regKeyInfo.rootKey, regKeyInfo.subKey.c_str(), 0, KEY_ALL_ACCESS, &hKey) != ERROR_SUCCESS) {
            TRACE_ERROR(L"Could not open key %s", regKeyInfo.subKey.c_str());
            continue;
        }

        // Update DefaultIcon value
        updateRegistryEntry(hKey, L"", value);
        if (RegCloseKey(hKey) != ERROR_SUCCESS) {
            TRACE_ERROR(L"Could not close key %s", regKeyInfo.subKey.c_str());
        }
    }
}

bool CloudProviderRegistrar::createRegistrationWithShell(ProviderInfo *providerInfo, std::wstring &syncRootID,
                                                         wchar_t *namespaceCLSID, DWORD *namespaceCLSIDSize) {
    TRACE_DEBUG(L"Registering new provider");
    if (!providerInfo->folderPath()) {
        TRACE_ERROR(L"Folder path is empty");
        return false;
    }

    if (!providerInfo->folderName()) {
        TRACE_ERROR(L"Folder name is empty");
        return false;
    }

    if (!providerInfo->id()) {
        TRACE_ERROR(L"Sync root id is empty");
        return false;
    }

    winrt::StorageProviderSyncRootInfo info;
    info.Id(syncRootID);

#ifndef NDEBUG
    // Silent WINRT_ASSERT(!is_sta())
    int reportMode = _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG);
#endif
    TRACE_DEBUG(L"Getting StorageFolder from path");
    auto folder = winrt::StorageFolder::GetFolderFromPathAsync(providerInfo->folderPath()).get();
#ifndef NDEBUG
    // Restore old report mode
    _CrtSetReportMode(_CRT_ASSERT, reportMode);
#endif

    info.Path(folder);

    info.DisplayNameResource(providerInfo->folderName());

    WCHAR exePath[MAX_FULL_PATH];
    TRACE_DEBUG(L"Getting module file name for icon resource");
    if (!GetModuleFileNameW(nullptr, exePath, MAX_FULL_PATH)) {
        TRACE_ERROR(L"Error in GetModuleFileNameW");
        return false;
    }
    info.IconResource(exePath); // App icon

    info.HydrationPolicy(winrt::StorageProviderHydrationPolicy::Full);
    info.HydrationPolicyModifier(winrt::StorageProviderHydrationPolicyModifier::AutoDehydrationAllowed);
    info.PopulationPolicy(winrt::StorageProviderPopulationPolicy::AlwaysFull);
    info.InSyncPolicy(winrt::StorageProviderInSyncPolicy::FileCreationTime |
                      winrt::StorageProviderInSyncPolicy::DirectoryCreationTime);
    info.Version(Utilities::s_version);
    info.ShowSiblingsAsGroup(false);
    info.HardlinkPolicy(winrt::StorageProviderHardlinkPolicy::None);

    wchar_t uriStr[MAX_URI];
    std::swprintf(uriStr, MAX_URI, Utilities::s_trashURI.c_str(), providerInfo->driveId());
    info.RecycleBinUri(winrt::Uri(uriStr));

    // Context
    std::wstring syncRootIdentity(providerInfo->id());

    TRACE_DEBUG(L"Converting sync root identity to binary");
    winrt::IBuffer contextBuffer =
            winrt::CryptographicBuffer::ConvertStringToBinary(syncRootIdentity.data(), winrt::BinaryStringEncoding::Utf8);
    info.Context(contextBuffer);

    if (!info.Path() || info.DisplayNameResource().empty() || info.Id().empty()) {
        TRACE_ERROR(L"Invalid StorageProviderSyncRootInfo");
        return false;
    }

    TRACE_DEBUG(L"Calling StorageProviderSyncRootManager::Register");
    winrt::StorageProviderSyncRootManager::Register(info);
    TRACE_DEBUG(L"Registered new provider with syncRootID=%s", syncRootID.c_str());
    // Give the cache some time to invalidate
    Sleep(1000);

    HKEY hKey;
    std::wstring subKey = REGPATH_SYNCROOTMANAGER + syncRootID;
    TRACE_DEBUG(L"Opening key %s", subKey.c_str());
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, subKey.c_str(), 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS) {
        TRACE_DEBUG(L"Opened key %s", subKey.c_str());
        if (namespaceCLSID) {
            TRACE_DEBUG(L"Getting NamespaceCLSID value");
            // Get CLSID
            if (RegGetValue(hKey, 0, REGKEY_NAMESPACECLSID, RRF_RT_ANY, nullptr, namespaceCLSID, namespaceCLSIDSize) !=
                ERROR_SUCCESS) {
                TRACE_ERROR(L"Could not get registry value NamespaceCLSID");
            }
        }
        TRACE_DEBUG(L"Setting registry values");
        // Set default key
        if (RegSetValueEx(hKey, nullptr, 0, REG_SZ, (BYTE *) Utilities::s_appName.c_str(),
                          (DWORD) (Utilities::s_appName.size() + 1) * sizeof(wchar_t)) != ERROR_SUCCESS) {
            TRACE_ERROR(L"Could not set default registry value");
        }

        // Create AMUID key
        const std::wstring name(REGKEY_AUMID);
        const std::wstring aumidValue = KDC_AUMID;
        const std::wstring value = L"Infomaniak.kDrive.Extension_" + aumidValue + L"!App";

        TRACE_INFO(L"AUMID value: %s", aumidValue.c_str());

        if (RegSetValueEx(hKey, name.c_str(), 0, REG_SZ, (const BYTE *) value.c_str(),
                          (DWORD) (value.size() + 1) * sizeof(wchar_t)) != ERROR_SUCCESS) {
            TRACE_ERROR(L"Could not set registry value %s=%s", name.c_str(), value.c_str());
        }

        TRACE_DEBUG(L"Closing key %s", subKey.c_str());
        if (RegCloseKey(hKey) != ERROR_SUCCESS) {
            TRACE_ERROR(L"Could not close key %s", subKey.c_str());
        }
    } else {
        TRACE_ERROR(L"Could not open key %s", subKey.c_str());
    }
    return true;
}

std::wstring CloudProviderRegistrar::registerWithShell(ProviderInfo *providerInfo, wchar_t *namespaceCLSID,
                                                       DWORD *namespaceCLSIDSize) {
    std::wstring syncRootID;

    try {
        syncRootID = getSyncRootId(providerInfo);
        if (syncRootID.empty()) {
            TRACE_ERROR(L"Error in getSyncRootId");
            return std::wstring();
        }
        TRACE_DEBUG(L"Registering sync root with ID: %s", syncRootID.c_str());
        TRACE_DEBUG(L"Registering sync root for folder path: %s", providerInfo->folderPath());
        // Find if the provider is already registered
        bool found(false);
        auto infoVector = winrt::StorageProviderSyncRootManager::GetCurrentSyncRoots();
        for (uint32_t i = 0; i < infoVector.Size(); i++) {
            if (syncRootID.compare(infoVector.GetAt(i).Id().c_str()) == 0) {
                found = true;
                break;
            }
        }

        if (found) {
            std::filesystem::path previousPath = getSyncRootPath(syncRootID);
            std::filesystem::path currentPath = providerInfo->folderPath();
            if (currentPath.lexically_normal() == previousPath.lexically_normal()) {
                updateRegistrationWithShell(syncRootID, namespaceCLSID, namespaceCLSIDSize);
                return syncRootID;
            }

            /* ORPHANED REGISTRY ENTRIES CLEANUP
             *
             * Problem:
             * Orphaned sync root entries can remain in the Windows registry when:
             *   1. During uninstallation, only the current user's sync roots are removed. Other users' LiteSync
             *      roots remain in the registry.
             *   2. If a user manually deletes the application database (%localappdata%/kDrive/parms.db), sync
             *      roots are never cleaned up from the registry.
             *
             * Impact:
             * A sync root is identified by its syncDbID, driveId, and Windows user SID. These orphaned entries
             * cause problems when the user later tries to sync the same drive to a different location. The
             * Windows API returns an error indicating the new folder is not a cloud sync root, because a registry
             * entry already exists with the same ID but a different path.
             *
             * Reproduction steps:
             *   1. Create a LiteSync synchronization.
             *   2. Close the app and delete parms.db.
             *   3. Try to create a new LiteSync synchronization.
             *   4. The sync fails to start.
             *
             * Solution:
             * On sync registration, verify that the registered path matches the current path. If they differ,
             * unregister the old sync root before registering the new one.
             */

            TRACE_INFO(L"Sync root path has changed from %s to %s, unregistering previous sync root before registering new one",
                       previousPath.c_str(), currentPath.c_str());
            if (!unregister(syncRootID)) {
                TRACE_ERROR(L"Could not unregister previous sync root with ID: %s", syncRootID.c_str());
            }
        }

        if (!createRegistrationWithShell(providerInfo, syncRootID, namespaceCLSID, namespaceCLSIDSize)) {
            TRACE_ERROR(L"Could not create registration with shell");
            return std::wstring();
        }

        return syncRootID;
    } catch (winrt::hresult_error const &ex) {
        TRACE_ERROR(L"Could not register the sync root, hr %08x - %s", static_cast<HRESULT>(winrt::to_hresult()),
                    ex.message().c_str());
        return std::wstring();
    } catch (std::exception const &ex) {
        TRACE_ERROR(L"Could not register the sync root, %ls", Utilities::utf8ToUtf16(ex.what()).c_str());
        return std::wstring();
    }
}

bool CloudProviderRegistrar::unregister(std::wstring syncRootID) {
    try {
        TRACE_DEBUG(L"StorageProviderSyncRootManager::Unregister: syncRootID = %ls", syncRootID.c_str());
        winrt::StorageProviderSyncRootManager::Unregister(syncRootID);
    } catch (winrt::hresult_error const &ex) {
        TRACE_ERROR(L"WinRT error caught : hr %08x - %s!", static_cast<HRESULT>(winrt::to_hresult()), ex.message().c_str());
        return false;
    }

    return true;
}

std::unique_ptr<TOKEN_USER> CloudProviderRegistrar::getTokenInformation() {
    std::unique_ptr<TOKEN_USER> tokenInfo;

    // get the tokenHandle from current thread/process if it's null
    auto tokenHandle{GetCurrentThreadEffectiveToken()}; // Pseudo token, don't free.

    DWORD tokenInfoSize{0};
    if (!::GetTokenInformation(tokenHandle, TokenUser, nullptr, 0, &tokenInfoSize)) {
        if (::GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
            tokenInfo.reset(reinterpret_cast<TOKEN_USER *>(new char[tokenInfoSize]));
            if (!::GetTokenInformation(tokenHandle, TokenUser, tokenInfo.get(), tokenInfoSize, &tokenInfoSize)) {
                throw std::exception("GetTokenInformation failed");
            }
        } else {
            throw std::exception("GetTokenInformation failed");
        }
    }
    return tokenInfo;
}

std::wstring CloudProviderRegistrar::getSyncRootId(const ProviderInfo *providerInfo) {
    std::unique_ptr<TOKEN_USER> tokenInfo(getTokenInformation());
    auto sidString = convertSidToStringSid(tokenInfo->User.Sid);
    std::wstring syncRootID(providerInfo->id());
    syncRootID.append(L"!");
    syncRootID.append(sidString.data());
    syncRootID.append(L"!");
    syncRootID.append(providerInfo->userId());

    return syncRootID;
}


std::filesystem::path CloudProviderRegistrar::getSyncRootPath(const std::wstring &syncRootID) {
    HKEY hKey = nullptr;
    const std::wstring subKey = REGPATH_SYNCROOTMANAGER + syncRootID + L"\\" + REGKEY_USERSYNCROOTS;

    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, subKey.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        TRACE_ERROR(L"Could not open key %s", subKey.c_str());
        return {};
    }

    std::filesystem::path syncRootPath;

    DWORD index = 0;
    wchar_t valueName[128] = {0};
    BYTE data[65534] = {0}; // Maximum path length in Windows is 32,767 characters (wchar are 2 bytes long)

    while (true) {
        DWORD valueNameSize = std::size(valueName);
        DWORD dataSize = sizeof(data);
        DWORD type = 0; // REG_SZ

        const LONG result = RegEnumValueW(hKey, index++, valueName, &valueNameSize, nullptr, &type, data, &dataSize);

        if (result == ERROR_NO_MORE_ITEMS) {
            TRACE_ERROR(L"The provided sync root ID %s does not have any path set", syncRootID.c_str());
            break;
        }

        if (result != ERROR_SUCCESS) {
            TRACE_ERROR(L"Could not enumerate registry value under %s", subKey.c_str());
            continue;
        }

        // Skip default value
        if (valueNameSize == 0) continue;

        if (type != REG_SZ) continue;


        (void) syncRootPath.assign(reinterpret_cast<wchar_t *>(data));

        if (!syncRootPath.has_root_path()) {
            TRACE_WARNING(L"Found a non path value in %s, this is not expected, continuing enumeration", subKey);
            continue;
        }

        TRACE_DEBUG(L"Found sync root path %s", syncRootPath.c_str());
    }

    TRACE_DEBUG(L"Closing key %s", subKey.c_str());

    if (RegCloseKey(hKey) != ERROR_SUCCESS) {
        TRACE_ERROR(L"Could not close key %s", subKey.c_str());
    }

    return syncRootPath;
}

winrt::com_array<wchar_t> CloudProviderRegistrar::convertSidToStringSid(PSID sid) {
    winrt::com_array<wchar_t> string;
    if (ConvertSidToStringSid(sid, winrt::put_abi(string))) {
        return string;
    } else {
        throw std::bad_alloc();
    }
}
