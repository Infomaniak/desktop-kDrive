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

#define REGKEY_NAMESPACECLSID L"NamespaceCLSID"
#define REGKEY_AUMID L"AUMID"

// Package family name, see FileExplorerExtensionPackage / Package.appxmanifest
/// EV (Extended Validation) certificate AUMID : use this certificate to build a release version
// #define REGVALUE_AUMID L"Infomaniak.kDrive.Extension_dbrs6rk4qqhna!App"
/// Virtual certificate AUMID : use this certificate in debug mode and to build a version for testing purpose
#define REGVALUE_AUMID L"Infomaniak.kDrive.Extension_csy8f8zhvqa20!App" // virtual

std::wstring CloudProviderRegistrar::registerWithShell(ProviderInfo *providerInfo, wchar_t *namespaceCLSID,
                                                       DWORD *namespaceCLSIDSize) {
    std::wstring syncRootID;

    try {
        syncRootID = getSyncRootId(providerInfo);

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
            HKEY hKey;
            std::wstring subKey = REGPATH_SYNCROOTMANAGER + syncRootID;
            if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, subKey.c_str(), 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS) {
                if (namespaceCLSID) {
                    // Get CLSID
                    if (RegGetValue(hKey, 0, REGKEY_NAMESPACECLSID, RRF_RT_ANY, nullptr, namespaceCLSID, namespaceCLSIDSize) !=
                        ERROR_SUCCESS) {
                        TRACE_ERROR(L"Could not get registry value NamespaceCLSID");
                    }
                }

                // Set default key
                if (RegSetValueEx(hKey, nullptr, 0, REG_SZ, (BYTE *) Utilities::s_appName.c_str(),
                                  (DWORD) (Utilities::s_appName.size() + 1) * sizeof(wchar_t)) != ERROR_SUCCESS) {
                    TRACE_ERROR(L"Could not set default registry value");
                }

                if (RegCloseKey(hKey) != ERROR_SUCCESS) {
                    TRACE_ERROR(L"Could not close key %s", subKey.c_str());
                }
            } else {
                TRACE_ERROR(L"Could not open key %s", subKey.c_str());
            }
        } else {
            winrt::StorageProviderSyncRootInfo info;
            info.Id(syncRootID);

            auto folder = winrt::StorageFolder::GetFolderFromPathAsync(providerInfo->folderPath()).get();
            info.Path(folder);

            info.DisplayNameResource(providerInfo->folderName());

            WCHAR exePath[MAX_FULL_PATH];
            GetModuleFileNameW(nullptr, exePath, MAX_FULL_PATH);
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

            winrt::IBuffer contextBuffer =
                    winrt::CryptographicBuffer::ConvertStringToBinary(syncRootIdentity.data(), winrt::BinaryStringEncoding::Utf8);
            info.Context(contextBuffer);

            winrt::StorageProviderSyncRootManager::Register(info);

            // Give the cache some time to invalidate
            Sleep(1000);

            HKEY hKey;
            std::wstring subKey = REGPATH_SYNCROOTMANAGER + syncRootID;
            if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, subKey.c_str(), 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS) {
                if (namespaceCLSID) {
                    // Get CLSID
                    if (RegGetValue(hKey, 0, REGKEY_NAMESPACECLSID, RRF_RT_ANY, nullptr, namespaceCLSID, namespaceCLSIDSize) !=
                        ERROR_SUCCESS) {
                        TRACE_ERROR(L"Could not get registry value NamespaceCLSID");
                    }
                }

                // Set default key
                if (RegSetValueEx(hKey, nullptr, 0, REG_SZ, (BYTE *) Utilities::s_appName.c_str(),
                                  (DWORD) (Utilities::s_appName.size() + 1) * sizeof(wchar_t)) != ERROR_SUCCESS) {
                    TRACE_ERROR(L"Could not set default registry value");
                }

                // Create AMUID key
                std::wstring name(REGKEY_AUMID);
                std::wstring value;
#ifdef _DEBUG
                DWORD aumidValueSize = 65535;
                std::wstring aumidValue;
                aumidValue.resize(aumidValueSize);
                aumidValueSize = GetEnvironmentVariableW(L"KDRIVEEXT_DEBUG_AUMID", &aumidValue[0], aumidValueSize);
                aumidValue.resize(aumidValueSize);
                value = L"Infomaniak.kDrive.Extension_" + aumidValue + L"!App";
                TRACE_INFO(L"AUMID value: %s", aumidValue.c_str());
#else
                value = REGVALUE_AUMID;
#endif
                if (RegSetValueEx(hKey, name.c_str(), 0, REG_SZ, (BYTE *) value.c_str(),
                                  (DWORD) (value.size() + 1) * sizeof(wchar_t)) != ERROR_SUCCESS) {
                    TRACE_ERROR(L"Could not set registry value %s=%s", name.c_str(), value.c_str());
                }

                if (RegCloseKey(hKey) != ERROR_SUCCESS) {
                    TRACE_ERROR(L"Could not close key %s", subKey.c_str());
                }
            } else {
                TRACE_ERROR(L"Could not open key %s", subKey.c_str());
            }
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

bool CloudProviderRegistrar::unregister(std::wstring syncRootID) {
    try {
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

winrt::com_array<wchar_t> CloudProviderRegistrar::convertSidToStringSid(PSID sid) {
    winrt::com_array<wchar_t> string;
    if (ConvertSidToStringSid(sid, winrt::put_abi(string))) {
        return string;
    } else {
        throw std::bad_alloc();
    }
}
