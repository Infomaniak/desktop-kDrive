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

std::wstring CloudProviderRegistrar::registerWithShell(ProviderInfo *providerInfo, wchar_t *namespaceCLSID,
                                                       DWORD *namespaceCLSIDSize) {
    std::wstring syncRootID;

    try {
        syncRootID = getSyncRootId(providerInfo);
        if (syncRootID.empty()) {
            TRACE_ERROR(L"Error in getSyncRootId");
            return std::wstring();
        }

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
            if (!providerInfo->folderPath()) {
                TRACE_ERROR(L"Folder path is empty");
                return std::wstring();
            }

            if (!providerInfo->folderName()) {
                TRACE_ERROR(L"Folder name is empty");
                return std::wstring();
            }

            if (!providerInfo->id()) {
                TRACE_ERROR(L"Sync root id is empty");
                return std::wstring();
            }

            winrt::StorageProviderSyncRootInfo info;
            info.Id(syncRootID);

#ifndef NDEBUG
            // Silent WINRT_ASSERT(!is_sta())
            int reportMode = _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG);
#endif
            auto folder = winrt::StorageFolder::GetFolderFromPathAsync(providerInfo->folderPath()).get();
#ifndef NDEBUG
            // Restore old report mode
            _CrtSetReportMode(_CRT_ASSERT, reportMode);
#endif

            info.Path(folder);

            info.DisplayNameResource(providerInfo->folderName());

            WCHAR exePath[MAX_FULL_PATH];
            if (!GetModuleFileNameW(nullptr, exePath, MAX_FULL_PATH)) {
                TRACE_ERROR(L"Error in GetModuleFileNameW");
                return std::wstring();
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

            winrt::IBuffer contextBuffer =
                    winrt::CryptographicBuffer::ConvertStringToBinary(syncRootIdentity.data(), winrt::BinaryStringEncoding::Utf8);
            info.Context(contextBuffer);

            if (!info.Path() || info.DisplayNameResource().empty() || info.Id().empty()) {
                TRACE_ERROR(L"Invalid StorageProviderSyncRootInfo");
                return std::wstring();
            }

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
                const std::wstring name(REGKEY_AUMID);
                const std::wstring aumidValue = KDC_AUMID;
                const std::wstring value = L"Infomaniak.kDrive.Extension_" + aumidValue + L"!App";

                TRACE_INFO(L"AUMID value: %s", aumidValue.c_str());

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

winrt::com_array<wchar_t> CloudProviderRegistrar::convertSidToStringSid(PSID sid) {
    winrt::com_array<wchar_t> string;
    if (ConvertSidToStringSid(sid, winrt::put_abi(string))) {
        return string;
    } else {
        throw std::bad_alloc();
    }
}
