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

#include "stdafx.h"

#include "KDOverlayRegistrationHandler.h"

#include <iostream>
#include <fstream>

using namespace std;

HRESULT KDOverlayRegistrationHandler::GetRegistryEntriesPrefix(PWSTR prefix) {
    if (prefix == nullptr) {
        return E_INVALIDARG;
    }
    // Reset the prefix to an empty string
    if (wcsncpy_s(prefix, MAX_PATH, L"", 0) != 0) {
        return E_FAIL;
    }
    HRESULT hResult = S_OK;
    HKEY shellOverlayKey = nullptr;
    // the key may not exist yet
    hResult = HRESULT_FROM_WIN32(RegOpenKeyEx(HKEY_LOCAL_MACHINE, REGISTRY_OVERLAY_KEY, 0, KEY_READ, &shellOverlayKey));
    if (!SUCCEEDED(hResult)) {
        if (hResult == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
            // If the key does not exist, we return S_OK with an empty prefix
            return S_OK;
        }
        return hResult;
    }

    // Get the name of the first subkey
    wchar_t subKeyName[MAX_PATH];
    if (wcsncpy_s(subKeyName, MAX_PATH, L"", 0) != 0) {
        return E_FAIL;
    }
    DWORD index = 0;
    DWORD dwSize = sizeof(subKeyName) / sizeof(wchar_t);
    hResult = HRESULT_FROM_WIN32(RegEnumKeyEx(shellOverlayKey, index, subKeyName, &dwSize, nullptr, nullptr, nullptr, nullptr));
    if (hResult != ERROR_SUCCESS) {
        // If there are no subkeys, we return S_OK with an empty prefix
        return S_OK;
    }

    // Count the numbre of ' ' characters at the beginning of the subkey name
    std::wstring subKeyNameW(subKeyName);
    size_t pos = subKeyNameW.find_first_not_of(L' ');
    if (pos == std::wstring::npos || pos > 30) {
        // If the subkey name is empty, contains only spaces or has more than 30 spaces at the beginning,
        // we return S_OK with an empty prefix
        return S_OK;
    }

    std::wstring spaces = subKeyNameW.substr(0, pos);
    // If the first key isn't a kDrive overlay add a space to the prefix
    if (subKeyNameW.find(OVERLAY_APP_NAME) == std::wstring::npos) {
        spaces += L" ";
        ++pos;
    }

    if (wcsncpy_s(prefix, MAX_PATH, spaces.c_str(), pos) != 0) {
        return E_FAIL;
    }

    return hResult;
}

HRESULT KDOverlayRegistrationHandler::MakeRegistryEntries(const CLSID &clsid, PCWSTR friendlyName) {
    HRESULT hResult;
    HKEY shellOverlayKey = nullptr;
    // the key may not exist yet
    hResult = HRESULT_FROM_WIN32(RegCreateKeyEx(HKEY_LOCAL_MACHINE, REGISTRY_OVERLAY_KEY, 0, nullptr, REG_OPTION_NON_VOLATILE,
                                                KEY_WRITE, nullptr, &shellOverlayKey, nullptr));
    if (!SUCCEEDED(hResult)) {
        hResult = RegCreateKey(HKEY_LOCAL_MACHINE, REGISTRY_OVERLAY_KEY, &shellOverlayKey);
        if (!SUCCEEDED(hResult)) {
            return hResult;
        }
    }

    HKEY syncExOverlayKey = nullptr;

    wchar_t prefix[MAX_PATH];
    hResult = GetRegistryEntriesPrefix(prefix);
    if (!SUCCEEDED(hResult)) {
        return hResult;
    }

    // Append the friendly name to the prefix
    std::wstring friendlyNameWithPrefix = std::wstring(prefix) + std::wstring(OVERLAY_APP_NAME) + std::wstring(friendlyName);

    hResult = HRESULT_FROM_WIN32(RegCreateKeyEx(shellOverlayKey, friendlyNameWithPrefix.c_str(), 0, nullptr,
                                                REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &syncExOverlayKey, nullptr));

    if (!SUCCEEDED(hResult)) {
        return hResult;
    }

    wchar_t stringCLSID[MAX_PATH];
    StringFromGUID2(clsid, stringCLSID, ARRAYSIZE(stringCLSID));
    LPCTSTR value = stringCLSID;
    hResult = RegSetValueEx(syncExOverlayKey, nullptr, 0, REG_SZ, (LPBYTE) value, (DWORD) ((wcslen(value) + 1) * sizeof(TCHAR)));
    if (!SUCCEEDED(hResult)) {
        return hResult;
    }

    return hResult;
}

HRESULT KDOverlayRegistrationHandler::RemoveRegistryEntries(PCWSTR friendlyName) {
    HRESULT hResult;
    HKEY shellOverlayKey = nullptr;
    hResult =
            HRESULT_FROM_WIN32(RegOpenKeyEx(HKEY_LOCAL_MACHINE, REGISTRY_OVERLAY_KEY, 0, KEY_WRITE | KEY_READ, &shellOverlayKey));

    if (!SUCCEEDED(hResult)) {
        return hResult;
    }

    wchar_t subKeyName[MAX_PATH];
    DWORD index = 0;
    DWORD dwSize = sizeof(subKeyName);
    while (RegEnumKeyEx(shellOverlayKey, index, subKeyName, &dwSize, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
        std::wstring friendlyNameW = std::wstring(OVERLAY_APP_NAME) + std::wstring(friendlyName);
        std::wstring subKeyNameW(subKeyName);
        if (subKeyNameW.ends_with(friendlyNameW)) {
            HKEY syncExOverlayKey = nullptr;
            hResult = HRESULT_FROM_WIN32(RegDeleteKey(shellOverlayKey, subKeyName));
        }
        index++;
        dwSize = sizeof(subKeyName);
    }
    return hResult;
}

HRESULT KDOverlayRegistrationHandler::RegisterCOMObject(PCWSTR modulePath, PCWSTR friendlyName, const CLSID &clsid) {
    if (modulePath == nullptr) {
        return E_FAIL;
    }

    wchar_t stringCLSID[MAX_PATH];
    StringFromGUID2(clsid, stringCLSID, ARRAYSIZE(stringCLSID));
    HRESULT hResult;
    HKEY hKey = nullptr;

    hResult = HRESULT_FROM_WIN32(RegOpenKeyEx(HKEY_CLASSES_ROOT, REGISTRY_CLSID, 0, KEY_WRITE, &hKey));
    if (!SUCCEEDED(hResult)) {
        return hResult;
    }

    HKEY clsidKey = nullptr;
    hResult = HRESULT_FROM_WIN32(
            RegCreateKeyEx(hKey, stringCLSID, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &clsidKey, nullptr));
    if (!SUCCEEDED(hResult)) {
        return hResult;
    }

    hResult = HRESULT_FROM_WIN32(RegSetValue(clsidKey, nullptr, REG_SZ, friendlyName, (DWORD) wcslen(friendlyName)));

    HKEY inprocessKey = nullptr;
    hResult = HRESULT_FROM_WIN32(RegCreateKeyEx(clsidKey, REGISTRY_IN_PROCESS, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE,
                                                nullptr, &inprocessKey, nullptr));
    if (!SUCCEEDED(hResult)) {
        return hResult;
    }

    hResult = HRESULT_FROM_WIN32(RegSetValue(inprocessKey, nullptr, REG_SZ, modulePath, (DWORD) wcslen(modulePath)));

    if (!SUCCEEDED(hResult)) {
        return hResult;
    }

    hResult = HRESULT_FROM_WIN32(RegSetValueEx(inprocessKey, REGISTRY_THREADING, 0, REG_SZ, (LPBYTE) REGISTRY_APARTMENT,
                                               (DWORD) ((wcslen(REGISTRY_APARTMENT) + 1) * sizeof(TCHAR))));
    if (!SUCCEEDED(hResult)) {
        return hResult;
    }

    hResult = HRESULT_FROM_WIN32(RegSetValueEx(inprocessKey, REGISTRY_VERSION, 0, REG_SZ, (LPBYTE) REGISTRY_VERSION_NUMBER,
                                               (DWORD) (wcslen(REGISTRY_VERSION_NUMBER) + 1) * sizeof(TCHAR)));
    if (!SUCCEEDED(hResult)) {
        return hResult;
    }

    return S_OK;
}

HRESULT KDOverlayRegistrationHandler::UnregisterCOMObject(const CLSID &clsid) {
    wchar_t stringCLSID[MAX_PATH];

    StringFromGUID2(clsid, stringCLSID, ARRAYSIZE(stringCLSID));
    HRESULT hResult;
    HKEY hKey = nullptr;
    hResult = HRESULT_FROM_WIN32(RegOpenKeyEx(HKEY_CLASSES_ROOT, REGISTRY_CLSID, 0, DELETE, &hKey));
    if (!SUCCEEDED(hResult)) {
        return hResult;
    }

    HKEY clsidKey = nullptr;
    hResult = HRESULT_FROM_WIN32(RegOpenKeyEx(hKey, stringCLSID, 0, DELETE, &clsidKey));
    if (!SUCCEEDED(hResult)) {
        return hResult;
    }

    hResult = HRESULT_FROM_WIN32(RegDeleteKey(clsidKey, REGISTRY_IN_PROCESS));
    if (!SUCCEEDED(hResult)) {
        return hResult;
    }

    hResult = HRESULT_FROM_WIN32(RegDeleteKey(hKey, stringCLSID));
    if (!SUCCEEDED(hResult)) {
        return hResult;
    }

    return S_OK;
}
