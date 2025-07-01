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

#include "utility.h"

#include "libcommon/utility/utility.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/io/iohelper_win.h"
#include "log/log.h"

#include <filesystem>
#include <string>
#include <iostream>
#include <system_error>

#include <windows.h>
#include <Shobjidl.h> //Required for IFileOperation Interface
#include <shellapi.h> //Required for Flags set in "SetOperationFlags"
#include <objbase.h>
#include <objidl.h>
#include <shlguid.h>
#include <strsafe.h>
#include <AclAPI.h>
#include <Accctrl.h>
#define SECURITY_WIN32
#include <security.h>

#include <log4cplus/loggingmacros.h>

#include <sentry.h>
#include <WinSock2.h>

constexpr int userNameBufLen = 4096;

namespace KDC {

static bool moveItemToTrash_private(const SyncPath &itemPath) {
    if (CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED) != S_OK) {
        LOGW_INFO(Log::instance()->getLogger(),
                  L"Error in CoInitializeEx in moveItemToTrash. Might be already initialized. Check if next call to "
                  L"CoCreateInstance is failing.");
    }

    // Create the IFileOperation object
    IFileOperation *fileOperation = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(FileOperation), NULL, CLSCTX_ALL, IID_PPV_ARGS(&fileOperation));
    if (FAILED(hr)) {
        // Couldn't CoCreateInstance - clean up and return
        LOGW_WARN(Log::instance()->getLogger(), L"Error in CoCreateInstance - path="
                                                        << Path2WStr(itemPath) << L" err="
                                                        << Utility::s2ws(std::system_category().message(hr)));

        std::wstringstream errorStream;
        errorStream << L"Move to trash failed for item with " << Utility::formatSyncPath(itemPath)
                    << L" - CoCreateInstance failed with error: " << Utility::s2ws(std::system_category().message(hr));
        std::wstring errorStr = errorStream.str();
        LOGW_WARN(Log::instance()->getLogger(), errorStr);
        sentry::Handler::captureMessage(sentry::Level::Error, "Utility::moveItemToTrash", "CoCreateInstance failed");
        CoUninitialize();
        return false;
    }

    hr = fileOperation->SetOperationFlags(FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT);
    if (FAILED(hr)) {
        // Couldn't add flags - clean up and return
        LOGW_WARN(Log::instance()->getLogger(), L"Error in SetOperationFlags path="
                                                        << Path2WStr(itemPath) << L" err="
                                                        << Utility::s2ws(std::system_category().message(hr)));

        std::wstringstream errorStream;
        errorStream << L"Move to trash failed for item " << Path2WStr(itemPath) << L" - SetOperationFlags failed with error: "
                    << Utility::s2ws(std::system_category().message(hr));
        std::wstring errorStr = errorStream.str();
        LOGW_WARN(Log::instance()->getLogger(), errorStr);

        sentry::Handler::captureMessage(sentry::Level::Error, "Utility::moveItemToTrash", "SetOperationFlags failed");
        fileOperation->Release();
        CoUninitialize();
        return false;
    }

    SyncPath itemPathPreferred(itemPath);
    IShellItem *fileOrFolderItem = NULL;
    hr = SHCreateItemFromParsingName(itemPathPreferred.make_preferred().native().c_str(), NULL, IID_PPV_ARGS(&fileOrFolderItem));
    if (FAILED(hr)) {
        // Couldn't get file into an item - cleanup and return (maybe the file doesn't exist?)
        LOGW_WARN(Log::instance()->getLogger(), L"Error in SHCreateItemFromParsingName - path="
                                                        << Path2WStr(itemPath) << L" err="
                                                        << Utility::s2ws(std::system_category().message(hr)));

        std::wstringstream errorStream;
        errorStream << L"Move to trash failed for item " << Path2WStr(itemPath)
                    << L" - SHCreateItemFromParsingName failed with error: " << Utility::s2ws(std::system_category().message(hr));
        std::wstring errorStr = errorStream.str();
        LOGW_WARN(Log::instance()->getLogger(), errorStr);

        sentry::Handler::captureMessage(sentry::Level::Error, "Utility::moveItemToTrash", "SHCreateItemFromParsingName failed");
        fileOperation->Release();
        CoUninitialize();
        return false;
    }

    hr = fileOperation->DeleteItem(fileOrFolderItem, NULL);
    if (FAILED(hr)) {
        // Failed to mark file/folder item for deletion - cleanup and return
        LOGW_WARN(Log::instance()->getLogger(), L"Error in DeleteItem - path="
                                                        << Path2WStr(itemPath) << L" err="
                                                        << Utility::s2ws(std::system_category().message(hr)));

        std::wstringstream errorStream;
        errorStream << L"Move to trash failed for item " << Path2WStr(itemPath) << L" - DeleteItem failed with error: "
                    << Utility::s2ws(std::system_category().message(hr));
        std::wstring errorStr = errorStream.str();
        LOGW_WARN(Log::instance()->getLogger(), errorStr);
        sentry::Handler::captureMessage(sentry::Level::Error, "Utility::moveItemToTrash", "DeleteItem failed");

        fileOrFolderItem->Release();
        fileOperation->Release();
        CoUninitialize();
        return false;
    }

    hr = fileOperation->PerformOperations();
    if (FAILED(hr)) {
        // failed to carry out delete - return
        LOGW_WARN(Log::instance()->getLogger(), L"Error in PerformOperations - path="
                                                        << Path2WStr(itemPath) << L" err="
                                                        << Utility::s2ws(std::system_category().message(hr)));

        std::wstringstream errorStream;
        errorStream << L"Move to trash failed for item " << Path2WStr(itemPath) << L" - PerformOperations failed with error: "
                    << Utility::s2ws(std::system_category().message(hr));
        std::wstring errorStr = errorStream.str();
        LOGW_WARN(Log::instance()->getLogger(), errorStr);

        sentry::Handler::captureMessage(sentry::Level::Error, "Utility::moveItemToTrash", "PerformOperations failed");
        fileOrFolderItem->Release();
        fileOperation->Release();
        CoUninitialize();
        return false;
    }

    BOOL aborted = false;
    hr = fileOperation->GetAnyOperationsAborted(&aborted);
    if (!FAILED(hr) && aborted) {
        // failed to carry out delete - return
        LOGW_WARN(Log::instance()->getLogger(), L"Error in GetAnyOperationsAborted - path="
                                                        << Path2WStr(itemPath) << L" err="
                                                        << Utility::s2ws(std::system_category().message(hr)));

        std::wstringstream errorStream;
        errorStream << L"Move to trash aborted for item " << Path2WStr(itemPath);
        std::wstring errorStr = errorStream.str();
        LOGW_WARN(Log::instance()->getLogger(), errorStr);

        sentry::Handler::captureMessage(sentry::Level::Error, "Utility::moveItemToTrash", "Move to trash aborted");

        fileOrFolderItem->Release();
        fileOperation->Release();
        CoUninitialize();
        return false;
    }

    fileOrFolderItem->Release();
    fileOperation->Release();
    CoUninitialize();
    return true;
}

static bool totalRamAvailable_private(uint64_t &ram, int &errorCode) {
    return true;
}

static bool ramCurrentlyUsed_private(uint64_t &ram, int &errorCode) {
    return true;
}

static bool ramCurrentlyUsedByProcess_private(uint64_t &ram, int &errorCode) {
    return true;
}

static bool cpuUsage_private(uint64_t &previousTotalTicks, uint64_t &previousIdleTicks, double &percent) {
    return true;
}

static bool cpuUsageByProcess_private(double &percent) {
    return true;
}

static std::string userName_private() {
    DWORD len = userNameBufLen;
    wchar_t userName[userNameBufLen];
    GetUserName(userName, &len);
    return Utility::ws2s(std::wstring(userName));
}

namespace {

LONG getMapFromRegistry(const HKEY &key, const std::wstring &subKey, std::unordered_map<std::wstring, std::wstring> &map) {
    map.clear();

    // Open the registry key
    HKEY hKey;
    LONG result = RegOpenKeyExW(key, subKey.c_str(), 0, KEY_READ, &hKey);
    if (result != ERROR_SUCCESS) {
        std::wcerr << L"Failed to open registry key, error: " << result << std::endl;
        return result;
    }

    DWORD index = 0;
    const DWORD maxValueNameSize = 16383; // max value name length
    const DWORD maxValueDataSize = 16383; // max data size
    wchar_t valueName[maxValueNameSize];
    BYTE valueData[maxValueDataSize];
    DWORD valueNameSize;
    DWORD valueDataSize;
    DWORD valueType;
    LONG res = ERROR_SUCCESS;

    while (true) {
        valueNameSize = maxValueNameSize;
        valueDataSize = maxValueDataSize;
        result = RegEnumValueW(hKey, index, valueName, &valueNameSize, nullptr, &valueType, valueData, &valueDataSize);
        if (result == ERROR_NO_MORE_ITEMS) {
            break; // no more values
        } else if (result != ERROR_SUCCESS) {
            std::wcerr << L"Failed to enumerate registry values, error: " << result << std::endl;
            res = result;
            break;
        }

        if (valueType == REG_SZ || valueType == REG_EXPAND_SZ) {
            // Data is a null-terminated string
            std::wstring dataStr(reinterpret_cast<wchar_t *>(valueData), valueDataSize / sizeof(wchar_t));
            // Remove trailing null characters
            size_t nullPos = dataStr.find(L'\0');
            if (nullPos != std::wstring::npos) {
                dataStr.resize(nullPos);
            }
            map.emplace(valueName, dataStr);
        } else {
            std::wcout << L"Value data type: " << valueType << L" (not displayed)" << std::endl;
        }
        ++index;
    }
    RegCloseKey(hKey);

    return res;
}

LONG setRegistryStringValue(const HKEY &key, const std::wstring &subKey, const std::wstring &valueName,
                            const std::wstring &valueData) {
    // Open or create the key with write access
    HKEY hKey;

    if (LONG result = RegCreateKeyExW(key, subKey.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr);
        result != ERROR_SUCCESS) {
        return result;
    }

    // Set the value
    if (LONG result = RegSetValueExW(hKey, valueName.c_str(), 0, REG_SZ, reinterpret_cast<const BYTE *>(valueData.c_str()),
                                     static_cast<DWORD>((valueData.size() + 1) * sizeof(wchar_t)));
        result != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return result;
    }

    RegCloseKey(hKey);
    return ERROR_SUCCESS;
}

LONG deleteRegistryValue(const HKEY &key, const std::wstring &subKey, const std::wstring &valueName) {
    // Open the key with write access
    HKEY hKey;
    if (LONG result = RegOpenKeyExW(key, subKey.c_str(), 0, KEY_SET_VALUE, &hKey); result != ERROR_SUCCESS) {
        return result;
    }

    // Delete the value
    if (LONG result = RegDeleteValueW(hKey, valueName.c_str()); result != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return result;
    }

    RegCloseKey(hKey);
    return ERROR_SUCCESS;
}

} // namespace

static const wchar_t systemRunPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

bool Utility::hasSystemLaunchOnStartup(const std::string &appName) {
    std::unordered_map<std::wstring, std::wstring> map;
    if (const auto result = getMapFromRegistry(HKEY_LOCAL_MACHINE, systemRunPath, map); result != ERROR_SUCCESS) {
        LOGW_WARN(logger(), L"Failed to retrieve registry values for: " << systemRunPath << L", error: " << result);
        return false;
    }

    return map.contains(Utility::s2ws(appName));
}

static const wchar_t runPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

bool Utility::hasLaunchOnStartup(const std::string &appName) {
    std::unordered_map<std::wstring, std::wstring> map;
    if (const auto result = getMapFromRegistry(HKEY_CURRENT_USER, runPath, map); result != ERROR_SUCCESS) {
        LOGW_WARN(logger(), L"Failed to retrieve registry values for: " << runPath << L", error: " << result);
        return false;
    }

    return map.contains(Utility::s2ws(appName));
}

bool Utility::setLaunchOnStartup(const std::string &appName, const std::string &guiName, bool enable) {
    if (enable) {
        SyncPath serverFilePath = KDC::CommonUtility::getAppWorkingDir() / (appName + ".exe");
        if (const auto result = setRegistryStringValue(HKEY_CURRENT_USER, runPath, Utility::s2ws(appName),
                                                       serverFilePath.make_preferred().native());
            result != ERROR_SUCCESS) {
            LOGW_WARN(logger(), L"Failed to set registry value for: " << systemRunPath << L", error: " << result);
            return false;
        }
    } else {
        if (const auto result = deleteRegistryValue(HKEY_CURRENT_USER, runPath, Utility::s2ws(appName));
            result != ERROR_SUCCESS) {
            LOGW_WARN(logger(), L"Failed to remove registry value for: " << systemRunPath << L", error: " << result);
            return false;
        }
    }
    return true;
}

} // namespace KDC
