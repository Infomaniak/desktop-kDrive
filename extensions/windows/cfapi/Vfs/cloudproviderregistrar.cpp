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

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Provider.h>
#include <winrt/Windows.Security.Cryptography.h>

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
#define REGKEY_ICONRESOURCE L"IconResource"
#define REGKEY_DEFAULTICON L"DefaultIcon"

constexpr const wchar_t *regKeyUserSyncRoots = L"UserSyncRoots";

// Number of 100ns intervals between 1601-01-01 (FILETIME epoch) and 1970-01-01 (Unix epoch)
constexpr uint64_t filetimeUnixEpochOffset = 116444736000000000ULL;
constexpr uint64_t filetimeTicksPerMillisecond = 10000ULL;

void updateRegistryEntry(const HKEY &hKey, const std::wstring &name, const std::wstring &value) {
    TRACE_INFO(L"%s value: %s", name.c_str(), value.c_str());

    if (RegSetValueEx(hKey, name.c_str(), 0, REG_SZ, (BYTE *) value.c_str(), (DWORD) (value.size() + 1) * sizeof(wchar_t)) !=
        ERROR_SUCCESS) {
        TRACE_ERROR(L"Could not set registry value %s=%s", name.c_str(), value.c_str());
    }
}

// Return the last write time of the UserSyncRoots registry key of the sync root, as a Unix timestamp (in milliseconds).
// This key is created when the sync root is registered and is not modified afterwards, so its last write time can be
// considered as the registration time of the sync root.
// Return 0 if the time cannot be retrieved.
static int64_t getRegistrationTime(const std::wstring &syncRootID) {
    const std::wstring subKey = REGPATH_SYNCROOTMANAGER + syncRootID + L"\\" + regKeyUserSyncRoots;
    HKEY hKey = nullptr;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, subKey.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        TRACE_ERROR(L"Could not open key %s", subKey.c_str());
        return 0;
    }

    FILETIME lastWriteTime{};
    const LSTATUS status = RegQueryInfoKey(hKey, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                                           nullptr, &lastWriteTime);
    if (RegCloseKey(hKey) != ERROR_SUCCESS) {
        TRACE_ERROR(L"Could not close key %s", subKey.c_str());
    }
    if (status != ERROR_SUCCESS) {
        TRACE_ERROR(L"Could not query info of key %s", subKey.c_str());
        return 0;
    }

    ULARGE_INTEGER ticks{};
    ticks.LowPart = lastWriteTime.dwLowDateTime;
    ticks.HighPart = lastWriteTime.dwHighDateTime;
    if (ticks.QuadPart < filetimeUnixEpochOffset) return 0;
    return static_cast<int64_t>((ticks.QuadPart - filetimeUnixEpochOffset) / filetimeTicksPerMillisecond);
}

std::wstring CloudProviderRegistrar::registerWithShell(ProviderInfo *providerInfo, wchar_t *namespaceCLSID,
                                                       DWORD *namespaceCLSIDSize, int64_t *registeredAt) {
    std::wstring syncRootID;
    if (registeredAt) *registeredAt = 0;

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
            // Update existing sync root registration (policies, version, identity) without unregistering it
            updateRegistration(providerInfo);

            updateSyncRootRegistryEntries(syncRootID, namespaceCLSID, namespaceCLSIDSize, true);
        } else {
            if (!createRegistration(providerInfo, syncRootID)) {
                return std::wstring();
            }

            updateSyncRootRegistryEntries(syncRootID, namespaceCLSID, namespaceCLSIDSize);
        }

        if (registeredAt) {
            *registeredAt = getRegistrationTime(syncRootID);
            TRACE_DEBUG(L"Sync root registration time: %lld", *registeredAt);
        }
    } catch (winrt::hresult_error const &ex) {
        TRACE_ERROR(L"Could not register the sync root, hr %08x - %s", static_cast<HRESULT>(winrt::to_hresult()),
                    ex.message().c_str());
        return std::wstring();
    } catch (std::exception const &ex) {
        TRACE_ERROR(L"Could not register the sync root, %ls", Utilities::utf8ToUtf16(ex.what()).c_str());
        return std::wstring();
    }

    return syncRootID;
}

void CloudProviderRegistrar::getCLSID(HKEY hKey, wchar_t *namespaceCLSID, DWORD *namespaceCLSIDSize) {
    // Get CLSID
    TRACE_DEBUG(L"Getting NamespaceCLSID value");
    if (RegGetValue(hKey, 0, REGKEY_NAMESPACECLSID, RRF_RT_ANY, nullptr, namespaceCLSID, namespaceCLSIDSize) != ERROR_SUCCESS) {
        TRACE_ERROR(L"Could not get registry value NamespaceCLSID");
    }
}

void CloudProviderRegistrar::updateAumidEntry(HKEY hKey) {
    // Update AUMID key
    const std::wstring aumidValue = KDC_AUMID;
    updateRegistryEntry(hKey, REGKEY_AUMID, L"Infomaniak.kDrive.Extension_" + aumidValue + L"!App");
}

void CloudProviderRegistrar::updateSyncRootRegistryEntries(const std::wstring &syncRootID, wchar_t *namespaceCLSID,
                                                           DWORD *namespaceCLSIDSize, bool updateIcons) {
    // Open the sync root registry key and refresh its entries (CLSID, default value, AUMID)
    HKEY hKey;
    const std::wstring subKey = REGPATH_SYNCROOTMANAGER + syncRootID;
    TRACE_DEBUG(L"Opening key %s", subKey.c_str());
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, subKey.c_str(), 0, KEY_ALL_ACCESS, &hKey) != ERROR_SUCCESS) {
        TRACE_ERROR(L"Could not open key %s", subKey.c_str());
        return;
    }

    TRACE_DEBUG(L"Opened key %s", subKey.c_str());
    if (namespaceCLSID) {
        getCLSID(hKey, namespaceCLSID, namespaceCLSIDSize);
    }

    TRACE_DEBUG(L"Setting registry values");
    // Set default key
    if (RegSetValueEx(hKey, nullptr, 0, REG_SZ, (BYTE *) Utilities::s_appName.c_str(),
                      (DWORD) (Utilities::s_appName.size() + 1) * sizeof(wchar_t)) != ERROR_SUCCESS) {
        TRACE_ERROR(L"Could not set default registry value");
    }

    updateAumidEntry(hKey);

    std::wstring value;
    if (updateIcons) {
        // Update IconResource
        std::wstring name(REGKEY_ICONRESOURCE);
        WCHAR exePath[MAX_FULL_PATH];
        if (!GetModuleFileNameW(nullptr, exePath, MAX_FULL_PATH)) {
            TRACE_ERROR(L"Error in GetModuleFileNameW");
        } else if (value = exePath; !value.empty()) {
            updateRegistryEntry(hKey, name, value);
        }
    }

    TRACE_DEBUG(L"Closing key %s", subKey.c_str());
    if (RegCloseKey(hKey) != ERROR_SUCCESS) {
        TRACE_ERROR(L"Could not close key %s", subKey.c_str());
    }

    if (updateIcons && namespaceCLSID) {
        // Update DefaultIcon keys. Reuses hKey, so it must run after the sync root key is closed
        struct RegKeyInfo {
                HKEY rootKey;
                std::wstring subKey;
        };
        std::vector<RegKeyInfo> regKeys = {
                {HKEY_CLASSES_ROOT,
                 REGPATH_HKEY_CLASSES_ROOT_CLSID + std::wstring(namespaceCLSID) + L"\\" + std::wstring(REGKEY_DEFAULTICON)},
                {HKEY_CLASSES_ROOT, REGPATH_HKEY_CLASSES_ROOT_WOW6432_CLSID + std::wstring(namespaceCLSID) + L"\\" +
                                            std::wstring(REGKEY_DEFAULTICON)},
                {HKEY_CURRENT_USER,
                 REGPATH_HKEY_CURRENT_USER_CLSID + std::wstring(namespaceCLSID) + L"\\" + std::wstring(REGKEY_DEFAULTICON)},
                {HKEY_CURRENT_USER, REGPATH_HKEY_CURRENT_USER_WOW6432_CLSID + std::wstring(namespaceCLSID) + L"\\" +
                                            std::wstring(REGKEY_DEFAULTICON)}};

        for (const auto &regKeyInfo: regKeys) {
            if (RegOpenKeyEx(regKeyInfo.rootKey, regKeyInfo.subKey.c_str(), 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS) {
                // Update DefaultIcon value
                updateRegistryEntry(hKey, L"", value);
                if (RegCloseKey(hKey) != ERROR_SUCCESS) {
                    TRACE_ERROR(L"Could not close key %s", regKeyInfo.subKey.c_str());
                }
            } else {
                TRACE_ERROR(L"Could not open key %s", regKeyInfo.subKey.c_str());
            }
        }
    }
}

void CloudProviderRegistrar::updateRegistration(ProviderInfo *providerInfo) {
    if (!providerInfo->folderPath()) {
        TRACE_ERROR(L"Folder path is empty");
        return;
    }

    // WARNING: keep these policies in sync with the ones set in createRegistration()
    const std::string syncRootIdentity = Utilities::utf16ToUtf8(providerInfo->id());
    CF_SYNC_REGISTRATION registration = {};
    registration.StructSize = sizeof(CF_SYNC_REGISTRATION);
    registration.ProviderName = Utilities::s_appName.c_str();
    registration.ProviderVersion = Utilities::s_version.c_str();
    registration.SyncRootIdentity = syncRootIdentity.c_str();
    registration.SyncRootIdentityLength = (DWORD) syncRootIdentity.size();

    CF_SYNC_POLICIES policies = {};
    policies.StructSize = sizeof(CF_SYNC_POLICIES);
    policies.Hydration.Primary = CF_HYDRATION_POLICY_FULL;
    policies.Hydration.Modifier = CF_HYDRATION_POLICY_MODIFIER_AUTO_DEHYDRATION_ALLOWED;
    policies.Population.Primary = CF_POPULATION_POLICY_ALWAYS_FULL;
    policies.Population.Modifier = CF_POPULATION_POLICY_MODIFIER_NONE;
    policies.InSync = CF_INSYNC_POLICY_TRACK_FILE_CREATION_TIME | CF_INSYNC_POLICY_TRACK_DIRECTORY_CREATION_TIME;
    policies.HardLink = CF_HARDLINK_POLICY_ALLOWED;

    TRACE_DEBUG(L"Updating sync root registration with CF_REGISTER_FLAG_UPDATE");
    const HRESULT hr = CfRegisterSyncRoot(providerInfo->folderPath(), &registration, &policies, CF_REGISTER_FLAG_UPDATE);
    if (FAILED(hr)) {
        TRACE_ERROR(L"Could not update the sync root registration, hr %08x", hr);
    }
}

bool CloudProviderRegistrar::createRegistration(ProviderInfo *providerInfo, const std::wstring &syncRootID) {
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

    // WARNING: keep these policies in sync with the ones set in updateRegistration()
    info.HydrationPolicy(winrt::StorageProviderHydrationPolicy::Full);
    info.HydrationPolicyModifier(winrt::StorageProviderHydrationPolicyModifier::AutoDehydrationAllowed);
    info.PopulationPolicy(winrt::StorageProviderPopulationPolicy::AlwaysFull);
    info.InSyncPolicy(winrt::StorageProviderInSyncPolicy::FileCreationTime |
                      winrt::StorageProviderInSyncPolicy::DirectoryCreationTime);
    info.Version(Utilities::s_version);
    info.ShowSiblingsAsGroup(false);
    info.HardlinkPolicy(winrt::StorageProviderHardlinkPolicy::Allowed);

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

    return true;
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

bool CloudProviderRegistrar::isRegistered(const std::wstring &providerId, const std::wstring &userId, const wchar_t *folderPath,
                                          bool &registered) {
    registered = false;

    std::wstring syncRootID;
    try {
        syncRootID = getSyncRootId(providerId, userId);
    } catch (const std::exception &) {
        TRACE_ERROR(L"Error in getSyncRootId");
        return false;
    }

    // The UserSyncRoots registry key of the sync root is removed when the sync root is unregistered (e.g. extension uninstall)
    const std::wstring subKey = REGPATH_SYNCROOTMANAGER + syncRootID + L"\\" + regKeyUserSyncRoots;
    HKEY hKey = nullptr;
    const LSTATUS status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, subKey.c_str(), 0, KEY_READ, &hKey);
    if (status == ERROR_FILE_NOT_FOUND) {
        TRACE_INFO(L"Sync root registry key not found: %ls", subKey.c_str());
        return true;
    }
    if (status != ERROR_SUCCESS) {
        TRACE_ERROR(L"Could not open key %ls, status %ld", subKey.c_str(), status);
        return false;
    }
    if (RegCloseKey(hKey) != ERROR_SUCCESS) {
        TRACE_ERROR(L"Could not close key %ls", subKey.c_str());
    }

    CF_SYNC_ROOT_BASIC_INFO info{};
    DWORD returnedLength = 0;
    const HRESULT hr = CfGetSyncRootInfoByPath(folderPath, CF_SYNC_ROOT_INFO_BASIC, &info, sizeof(info), &returnedLength);
    if (hr == HRESULT_FROM_WIN32(ERROR_CLOUD_FILE_NOT_UNDER_SYNC_ROOT)) {
        TRACE_INFO(L"Folder is not under a sync root anymore: %ls", folderPath);
        return true;
    }
    if (FAILED(hr)) {
        TRACE_ERROR(L"Error in CfGetSyncRootInfoByPath: %ls, hr %08x", folderPath, hr);
        return false;
    }

    registered = true;
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
    return getSyncRootId(providerInfo->id(), providerInfo->userId());
}

std::wstring CloudProviderRegistrar::getSyncRootId(const std::wstring &providerId, const std::wstring &userId) {
    std::unique_ptr<TOKEN_USER> tokenInfo(getTokenInformation());
    auto sidString = convertSidToStringSid(tokenInfo->User.Sid);
    std::wstring syncRootID(providerId);
    syncRootID.append(L"!");
    syncRootID.append(sidString.data());
    syncRootID.append(L"!");
    syncRootID.append(userId);

    return syncRootID;
}

winrt::com_array<wchar_t> CloudProviderRegistrar::convertSidToStringSid(PSID sid) {
    winrt::com_array<wchar_t> string;
    if (ConvertSidToStringSid(sid, winrt::put_abi(string))) {
        return string;
    } else {
        throw std::bad_alloc();
    }
}
