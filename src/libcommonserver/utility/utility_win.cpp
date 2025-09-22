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
#include "log/log.h"

#include <filesystem>
#include <string>
#include <iostream>
#include <system_error>

#include <windows.h>
#include <Shobjidl.h> //Required for IFileOperation Interface
#include <shellapi.h> //Required for Flags set in "SetOperationFlags"
#include <shlobj_core.h> // SHCreateItemFromIDList
#include <atlbase.h> // CComPtr
#include <objbase.h>
#include <objidl.h>
#include <shlguid.h>
#include <strsafe.h>
#include <AclAPI.h>
#include <Accctrl.h>
#define SECURITY_WIN32
#include "utility/logiffail.h"


#include <security.h>

#include <log4cplus/loggingmacros.h>

#include <sentry.h>
#include <WinSock2.h>

constexpr int userNameBufLen = 4096;

namespace KDC {

bool Utility::moveItemToTrash(const SyncPath &itemPath) {
    if (CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED) != S_OK) {
        LOGW_INFO(Log::instance()->getLogger(),
                  L"Error in CoInitializeEx in moveItemToTrash. Might be already initialized. Check if next call to "
                  L"CoCreateInstance is failing.");
    }

    // Create the IFileOperation object
    IFileOperation *fileOperation = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(FileOperation), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&fileOperation));
    if (FAILED(hr)) {
        // Couldn't CoCreateInstance - clean up and return
        LOGW_WARN(Log::instance()->getLogger(), L"Error in CoCreateInstance - path="
                                                        << Path2WStr(itemPath) << L" err="
                                                        << CommonUtility::s2ws(std::system_category().message(hr)));

        std::wstringstream errorStream;
        errorStream << L"Move to trash failed for item with " << Utility::formatSyncPath(itemPath)
                    << L" - CoCreateInstance failed with error: " << CommonUtility::s2ws(std::system_category().message(hr));
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
                                                        << CommonUtility::s2ws(std::system_category().message(hr)));

        std::wstringstream errorStream;
        errorStream << L"Move to trash failed for item " << Path2WStr(itemPath) << L" - SetOperationFlags failed with error: "
                    << CommonUtility::s2ws(std::system_category().message(hr));
        std::wstring errorStr = errorStream.str();
        LOGW_WARN(Log::instance()->getLogger(), errorStr);

        sentry::Handler::captureMessage(sentry::Level::Error, "Utility::moveItemToTrash", "SetOperationFlags failed");
        fileOperation->Release();
        CoUninitialize();
        return false;
    }

    SyncPath itemPathPreferred(itemPath);
    IShellItem *fileOrFolderItem = nullptr;
    hr = SHCreateItemFromParsingName(itemPathPreferred.make_preferred().native().c_str(), nullptr,
                                     IID_PPV_ARGS(&fileOrFolderItem));
    if (FAILED(hr)) {
        // Couldn't get file into an item - cleanup and return (maybe the file doesn't exist?)
        LOGW_WARN(Log::instance()->getLogger(), L"Error in SHCreateItemFromParsingName - path="
                                                        << Path2WStr(itemPath) << L" err="
                                                        << CommonUtility::s2ws(std::system_category().message(hr)));

        std::wstringstream errorStream;
        errorStream << L"Move to trash failed for item " << Path2WStr(itemPath)
                    << L" - SHCreateItemFromParsingName failed with error: "
                    << CommonUtility::s2ws(std::system_category().message(hr));
        std::wstring errorStr = errorStream.str();
        LOGW_WARN(Log::instance()->getLogger(), errorStr);

        sentry::Handler::captureMessage(sentry::Level::Error, "Utility::moveItemToTrash", "SHCreateItemFromParsingName failed");
        fileOperation->Release();
        CoUninitialize();
        return false;
    }

    hr = fileOperation->DeleteItem(fileOrFolderItem, nullptr);
    if (FAILED(hr)) {
        // Failed to mark file/folder item for deletion - cleanup and return
        LOGW_WARN(Log::instance()->getLogger(), L"Error in DeleteItem - path="
                                                        << Path2WStr(itemPath) << L" err="
                                                        << CommonUtility::s2ws(std::system_category().message(hr)));

        std::wstringstream errorStream;
        errorStream << L"Move to trash failed for item " << Path2WStr(itemPath) << L" - DeleteItem failed with error: "
                    << CommonUtility::s2ws(std::system_category().message(hr));
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
                                                        << CommonUtility::s2ws(std::system_category().message(hr)));

        std::wstringstream errorStream;
        errorStream << L"Move to trash failed for item " << Path2WStr(itemPath) << L" - PerformOperations failed with error: "
                    << CommonUtility::s2ws(std::system_category().message(hr));
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
                                                        << CommonUtility::s2ws(std::system_category().message(hr)));

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

bool Utility::totalRamAvailable(uint64_t &ram, int &errorCode) {
    return true;
}

bool Utility::ramCurrentlyUsed(uint64_t &ram, int &errorCode) {
    return true;
}

bool Utility::ramCurrentlyUsedByProcess(uint64_t &ram, int &errorCode) {
    return true;
}

bool Utility::cpuUsage(uint64_t &previousTotalTicks, uint64_t &previousIdleTicks, double &percent) {
    return true;
}

bool Utility::cpuUsageByProcess(double &percent) {
    return true;
}

std::string Utility::userName() {
    DWORD len = userNameBufLen;
    wchar_t userName[userNameBufLen];
    GetUserName(userName, &len);
    return CommonUtility::ws2s(std::wstring(userName));
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

    return map.contains(CommonUtility::s2ws(appName));
}

static const wchar_t runPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

bool Utility::hasLaunchOnStartup(const std::string &appName) {
    std::unordered_map<std::wstring, std::wstring> map;
    if (const auto result = getMapFromRegistry(HKEY_CURRENT_USER, runPath, map); result != ERROR_SUCCESS) {
        LOGW_WARN(logger(), L"Failed to retrieve registry values for: " << runPath << L", error: " << result);
        return false;
    }

    return map.contains(CommonUtility::s2ws(appName));
}

bool Utility::setLaunchOnStartup(const std::string &appName, const std::string &guiName, bool enable) {
    if (enable) {
        SyncPath serverFilePath = KDC::CommonUtility::getAppWorkingDir() / (appName + ".exe");
        if (const auto result = setRegistryStringValue(HKEY_CURRENT_USER, runPath, CommonUtility::s2ws(appName),
                                                       serverFilePath.make_preferred().native());
            result != ERROR_SUCCESS) {
            LOGW_WARN(logger(), L"Failed to set registry value for: " << systemRunPath << L", error: " << result);
            return false;
        }
    } else {
        if (const auto result = deleteRegistryValue(HKEY_CURRENT_USER, runPath, CommonUtility::s2ws(appName));
            result != ERROR_SUCCESS) {
            LOGW_WARN(logger(), L"Failed to remove registry value for: " << systemRunPath << L", error: " << result);
            return false;
        }
    }
    return true;
}

void Utility::setFolderPinState(const std::wstring &clsid, bool show) {
    std::wstring clsidPath = L"Software\\Classes\\CLSID\\" + clsid;
    std::wstring clsidPathWow64 = L"Software\\Classes\\Wow6432Node\\CLSID\\" + clsid;

    std::wstring error;
    if (Utility::registryExistKeyTree(HKEY_CURRENT_USER, clsidPath)) {
        Utility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPath, L"System.IsPinnedToNameSpaceTree", REG_DWORD, show, error);
    }

    if (Utility::registryExistKeyTree(HKEY_CURRENT_USER, clsidPathWow64)) {
        Utility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPathWow64, L"System.IsPinnedToNameSpaceTree", REG_DWORD, show,
                                     error);
    }
}

// Add legacy sync root keys
void Utility::addLegacySyncRootKeys(const std::wstring &clsid, const SyncPath &folderPath, bool show) {
    const std::wstring clsidPath = L"Software\\Classes\\CLSID\\" + clsid;
    const std::wstring clsidPathWow64 = L"Software\\Classes\\Wow6432Node\\CLSID\\" + clsid;
    const std::wstring namespacePath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Desktop\\NameSpace\\" + clsid;

    SyncName title = folderPath.filename().native();
    std::wstring iconPath = CommonUtility::applicationFilePath().native();
    std::wstring targetFolderPath = SyncPath(folderPath).make_preferred().native();

    //  Steps taken from: https://msdn.microsoft.com/en-us/library/windows/desktop/dn889934%28v=vs.85%29.aspx
    //  Step 1: Add your CLSID and name your extension
    std::wstring error;
    (void) Utility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPath, {}, REG_SZ, title, error);
    (void) Utility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPathWow64, {}, REG_SZ, title, error);
    // Step 2: Set the image for your icon
    (void) Utility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPath + L"\\DefaultIcon", {}, REG_SZ, iconPath, error);
    (void) Utility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPathWow64 + L"\\DefaultIcon", {}, REG_SZ, iconPath, error);
    // Step 3: Add your extension to the Navigation Pane and make it visible
    (void) Utility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPath, L"System.IsPinnedToNameSpaceTree", REG_DWORD, show, error);
    (void) Utility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPathWow64, L"System.IsPinnedToNameSpaceTree", REG_DWORD, show,
                                        error);
    // Step 4: Set the location for your extension in the Navigation Pane
    (void) Utility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPath, L"SortOrderIndex", REG_DWORD, 0x41, error);
    (void) Utility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPathWow64, L"SortOrderIndex", REG_DWORD, 0x41, error);
    // Step 5: Provide the dll that hosts your extension.
    (void) Utility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPath + L"\\InProcServer32", {}, REG_EXPAND_SZ,
                                        L"%systemroot%\\system32\\shell32.dll", error);
    (void) Utility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPathWow64 + L"\\InProcServer32", {}, REG_EXPAND_SZ,
                                        L"%systemroot%\\system32\\shell32.dll", error);
    // Step 6: Define the instance object
    // Indicate that your namespace extension should function like other file folder structures in File Explorer.
    (void) Utility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPath + L"\\Instance", L"CLSID", REG_SZ,
                                        L"{0E5AAE11-A475-4c5b-AB00-C66DE400274E}", error);
    (void) Utility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPathWow64 + L"\\Instance", L"CLSID", REG_SZ,
                                        L"{0E5AAE11-A475-4c5b-AB00-C66DE400274E}", error);
    // Step 7: Provide the file system attributes of the target folder
    (void) Utility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPath + L"\\Instance\\InitPropertyBag", L"Attributes", REG_DWORD,
                                        0x11, error);
    (void) Utility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPathWow64 + L"\\Instance\\InitPropertyBag", L"Attributes",
                                        REG_DWORD, 0x11, error);
    // Step 8: Set the path for the sync root
    (void) Utility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPath + L"\\Instance\\InitPropertyBag", L"TargetFolderPath",
                                        REG_SZ, targetFolderPath, error);
    (void) Utility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPathWow64 + L"\\Instance\\InitPropertyBag", L"TargetFolderPath",
                                        REG_SZ, targetFolderPath, error);
    // Step 9: Set appropriate shell flags
    (void) Utility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPath + L"\\ShellFolder", L"FolderValueFlags", REG_DWORD, 0x28,
                                        error);
    (void) Utility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPathWow64 + L"\\ShellFolder", L"FolderValueFlags", REG_DWORD,
                                        0x28, error);
    // Step 10: Set the appropriate flags to control your shell behavior
    (void) Utility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPath + L"\\ShellFolder", L"Attributes", REG_DWORD,
                                        static_cast<int>(0xF080004DU), error);
    (void) Utility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPathWow64 + L"\\ShellFolder", L"Attributes", REG_DWORD,
                                        static_cast<int>(0xF080004DU), error);
    // Step 11: Register your extension in the namespace root
    (void) Utility::registrySetKeyValue(HKEY_CURRENT_USER, namespacePath, {}, REG_SZ, title, error);
    // Step 12: Hide your extension from the Desktop
    (void) Utility::registrySetKeyValue(
            HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\HideDesktopIcons\\NewStartPanel", clsid,
            REG_DWORD, 0x1, error);

    // For us, to later be able to iterate and find our own namespace entries and associated CLSID.
    // Use the macro instead of the theme to make sure it matches with the uninstaller.
    (void) Utility::registrySetKeyValue(HKEY_CURRENT_USER, namespacePath, L"ApplicationName", REG_SZ,
                                        CommonUtility::s2ws(APPLICATION_NAME), error);
}

// Remove legacy sync root keys
void Utility::removeLegacySyncRootKeys(const std::wstring &clsid) {
    const std::wstring clsidPath = L"Software\\Classes\\CLSID\\" + clsid;
    const std::wstring clsidPathWow64 = L"Software\\Classes\\Wow6432Node\\CLSID\\" + clsid;
    const std::wstring namespacePath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Desktop\\NameSpace\\" + clsid;
    const std::wstring newstartpanelPath =
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\HideDesktopIcons\\NewStartPanel";

    if (Utility::registryExistKeyTree(HKEY_CURRENT_USER, clsidPath)) {
        Utility::registryDeleteKeyTree(HKEY_CURRENT_USER, clsidPath);
    }
    if (Utility::registryExistKeyTree(HKEY_CURRENT_USER, clsidPathWow64)) {
        Utility::registryDeleteKeyTree(HKEY_CURRENT_USER, clsidPathWow64);
    }
    if (Utility::registryExistKeyTree(HKEY_CURRENT_USER, namespacePath)) {
        Utility::registryDeleteKeyTree(HKEY_CURRENT_USER, namespacePath);
    }
    if (Utility::registryExistKeyValue(HKEY_CURRENT_USER, newstartpanelPath, clsid)) {
        Utility::registryDeleteKeyValue(HKEY_CURRENT_USER, newstartpanelPath, clsid);
    }
}

bool Utility::registryExistKeyTree(HKEY hRootKey, const std::wstring &subKey) {
    HKEY hKey;

    REGSAM sam = KEY_READ | KEY_WOW64_64KEY;
    LONG result = RegOpenKeyEx(hRootKey, subKey.c_str(), 0, sam, &hKey);
    LOG_IF_FAIL(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND)
    if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) return false;

    RegCloseKey(hKey);

    return result == ERROR_SUCCESS;
}

bool Utility::registryExistKeyValue(HKEY hRootKey, const std::wstring &subKey, const std::wstring &valueName) {
    HKEY hKey;

    REGSAM sam = KEY_READ | KEY_WOW64_64KEY;
    LONG result = RegOpenKeyEx(hRootKey, subKey.c_str(), 0, sam, &hKey);
    LOG_IF_FAIL(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND)
    if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) return false;

    DWORD type = 0, sizeInBytes = 0;
    result = RegQueryValueEx(hKey, valueName.c_str(), 0, &type, nullptr, &sizeInBytes);
    LOG_IF_FAIL(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND)
    if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) return false;

    RegCloseKey(hKey);

    return result == ERROR_SUCCESS;
}

Utility::kdVariant Utility::registryGetKeyValue(const HKEY hRootKey, const std::wstring &subKey, const std::wstring &valueName) {
    kdVariant value;

    HKEY hKey = nullptr;

    const REGSAM sam = KEY_READ | KEY_WOW64_64KEY;
    LONG result = RegOpenKeyEx(hRootKey, subKey.c_str(), 0, sam, &hKey);
    LOG_IF_FAIL(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND)
    if (result != ERROR_SUCCESS) return value;

    DWORD type = 0;
    DWORD sizeInBytes = 0;
    result = RegQueryValueEx(hKey, valueName.c_str(), 0, &type, nullptr, &sizeInBytes);
    LOG_IF_FAIL(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND)
    if (result != ERROR_SUCCESS) {
        (void) RegCloseKey(hKey);
        return value;
    }

    switch (type) {
        case REG_DWORD: {
            DWORD dword = 0;
            Q_ASSERT(sizeInBytes == sizeof(dword));
            if (RegQueryValueEx(hKey, valueName.c_str(), 0, &type, reinterpret_cast<LPBYTE>(&dword), &sizeInBytes) ==
                ERROR_SUCCESS) {
                value = int(dword);
            }
            break;
        }
        case REG_EXPAND_SZ:
        case REG_SZ: {
            std::wstring string;
            string.resize(sizeInBytes / sizeof(QChar));
            result = RegQueryValueEx(hKey, valueName.c_str(), 0, &type, reinterpret_cast<LPBYTE>(string.data()), &sizeInBytes);

            if (result == ERROR_SUCCESS) {
                const int newCharSize = sizeInBytes / sizeof(QChar);
                // From the doc:
                // If the data has the REG_SZ, REG_MULTI_SZ or REG_EXPAND_SZ type, the string may not have been stored with
                // the proper terminating null characters. Therefore, even if the function returns ERROR_SUCCESS,
                // the application should ensure that the string is properly terminated before using it; otherwise, it may
                // overwrite a buffer.
                if (string.at(newCharSize - 1) == QChar('\0')) string.resize(newCharSize - 1);
                value = string;
            }
            break;
        }
        default:
            Q_UNREACHABLE();
    }
    LOG_IF_FAIL(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND)

    RegCloseKey(hKey);
    return value;
}

bool Utility::registrySetKeyValue(HKEY hRootKey, const std::wstring &subKey, const std::wstring &valueName, DWORD type,
                                  const kdVariant &value, std::wstring &error) {
    HKEY hKey;
    // KEY_WOW64_64KEY is necessary because CLSIDs are "Redirected and reflected only for CLSIDs that do not specify
    // InprocServer32 or InprocHandler32."
    // https://msdn.microsoft.com/en-us/library/windows/desktop/aa384253%28v=vs.85%29.aspx#redirected__shared__and_reflected_keys_under_wow64
    // This shouldn't be an issue in our case since we use shell32.dll as InprocServer32, so we could write those registry keys
    // for both 32 and 64bit.
    REGSAM sam = KEY_WRITE | KEY_WOW64_64KEY;
    LONG result = RegCreateKeyEx(hRootKey, subKey.c_str(), 0, nullptr, 0, sam, nullptr, &hKey, nullptr);
    LOG_IF_FAIL(result == ERROR_SUCCESS)
    if (result != ERROR_SUCCESS) {
        LPTSTR errorText = nullptr;
        if (!FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
                           result, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                           (LPTSTR) &errorText, // output
                           0, nullptr)) {
            error = L"Format message failed";
        } else {
            error = errorText;
        }

        return false;
    }

    result = -1;
    switch (type) {
        case REG_DWORD: {
            DWORD dword = std::get<int>(value);
            result = RegSetValueEx(hKey, valueName.c_str(), 0, type, reinterpret_cast<const BYTE *>(&dword), sizeof(dword));
            break;
        }
        case REG_EXPAND_SZ:
        case REG_SZ: {
            std::wstring string = std::get<std::wstring>(value);
            result = RegSetValueEx(hKey, valueName.c_str(), 0, type, reinterpret_cast<const BYTE *>(string.c_str()),
                                   ((DWORD) string.size() + 1) * sizeof(QChar));
            break;
        }
        default:
            Q_UNREACHABLE();
    }
    LOG_IF_FAIL(result == ERROR_SUCCESS)
    if (result != ERROR_SUCCESS) {
        LPTSTR errorText = nullptr;
        if (!FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
                           result, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                           (LPTSTR) &errorText, // output
                           0, nullptr)) {
            error = L"Format message failed";
        } else {
            error = *errorText;
        }

        return false;
    }

    RegCloseKey(hKey);
    return result == ERROR_SUCCESS;
}

bool Utility::registryDeleteKeyTree(HKEY hRootKey, const std::wstring &subKey) {
    HKEY hKey;
    REGSAM sam = DELETE | KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_64KEY;
    LONG result = RegOpenKeyEx(hRootKey, subKey.c_str(), 0, sam, &hKey);
    LOG_IF_FAIL(result == ERROR_SUCCESS)
    if (result != ERROR_SUCCESS) return false;

    result = RegDeleteTree(hKey, nullptr);
    RegCloseKey(hKey);
    LOG_IF_FAIL(result == ERROR_SUCCESS)

    result |= RegDeleteKeyEx(hRootKey, subKey.c_str(), sam, 0);
    LOG_IF_FAIL(result == ERROR_SUCCESS)

    return result == ERROR_SUCCESS;
}

bool Utility::registryDeleteKeyValue(HKEY hRootKey, const std::wstring &subKey, const std::wstring &valueName) {
    HKEY hKey;
    REGSAM sam = KEY_WRITE | KEY_WOW64_64KEY;
    LONG result = RegOpenKeyEx(hRootKey, subKey.c_str(), 0, sam, &hKey);
    LOG_IF_FAIL(result == ERROR_SUCCESS)
    if (result != ERROR_SUCCESS) return false;

    result = RegDeleteValue(hKey, valueName.c_str());
    LOG_IF_FAIL(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND)

    RegCloseKey(hKey);
    return result == ERROR_SUCCESS;
}

bool Utility::registryWalkSubKeys(HKEY hRootKey, const std::wstring &subKey,
                                  const std::function<void(HKEY, const std::wstring &)> &callback) {
    HKEY hKey;
    REGSAM sam = KEY_READ | KEY_WOW64_64KEY;
    LONG result = RegOpenKeyEx(hRootKey, subKey.c_str(), 0, sam, &hKey);
    LOG_IF_FAIL(result == ERROR_SUCCESS)
    if (result != ERROR_SUCCESS) return false;

    DWORD maxSubKeyNameSize;
    // Get the largest keyname size once instead of relying each call on ERROR_MORE_DATA.
    result = RegQueryInfoKey(hKey, nullptr, nullptr, nullptr, nullptr, &maxSubKeyNameSize, nullptr, nullptr, nullptr, nullptr,
                             nullptr, nullptr);
    LOG_IF_FAIL(result == ERROR_SUCCESS)
    if (result != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return false;
    }

    std::wstring subKeyName;
    subKeyName.reserve(maxSubKeyNameSize + 1);

    DWORD retCode = ERROR_SUCCESS;
    for (DWORD i = 0; retCode == ERROR_SUCCESS; ++i) {
        Q_ASSERT(unsigned(subKeyName.capacity()) > maxSubKeyNameSize);
        // Make the previously reserved capacity official again.
        subKeyName.resize(subKeyName.capacity());
        DWORD subKeyNameSize = subKeyName.size();
        retCode = RegEnumKeyEx(hKey, i, reinterpret_cast<LPWSTR>(subKeyName.data()), &subKeyNameSize, nullptr, nullptr, nullptr,
                               nullptr);

        LOG_IF_FAIL(result == ERROR_SUCCESS || retCode == ERROR_NO_MORE_ITEMS)
        if (retCode == ERROR_SUCCESS) {
            // subKeyNameSize excludes the trailing \0
            subKeyName.resize(subKeyNameSize);
            // Pass only the sub keyname, not the full path.
            callback(hKey, subKeyName);
        }
    }

    RegCloseKey(hKey);
    return retCode != ERROR_NO_MORE_ITEMS;
}

void Utility::unixTimeToFiletime(time_t t, FILETIME *filetime) {
    LONGLONG ll = Int32x32To64(t, 10000000) + 116444736000000000;
    filetime->dwLowDateTime = (DWORD) ll;
    filetime->dwHighDateTime = ll >> 32;
}

namespace {
HRESULT bindToCsidl(const int csidl, REFIID riid, void **ppv) {
    auto hr = S_OK;
    PIDLIST_ABSOLUTE pidl = nullptr;
    hr = SHGetSpecialFolderLocation(nullptr, csidl, &pidl);
    if (SUCCEEDED(hr)) {
        IShellFolder *psfDesktop = nullptr;
        hr = SHGetDesktopFolder(&psfDesktop);
        if (SUCCEEDED(hr)) {
            if (pidl->mkid.cb) {
                hr = psfDesktop->BindToObject(pidl, nullptr, riid, ppv);
            } else {
                hr = psfDesktop->QueryInterface(riid, ppv);
            }
            (void) psfDesktop->Release();
        }
        CoTaskMemFree(pidl);
    }
    return hr;
}

bool isFolder(IShellItem *item) {
    if (!item) return false;

    SFGAOF attributes = SFGAO_FOLDER;
    if (const HRESULT hr = item->GetAttributes(SFGAO_FOLDER, &attributes); SUCCEEDED(hr)) {
        return (attributes & SFGAO_FOLDER) != 0;
    }

    return false;
}

bool isInFolder(const std::list<SyncName> &relativePath, IShellFolder2 *folder) {
    IEnumIDList *peidl = nullptr;

    if (const auto enumHr = folder->EnumObjects(nullptr, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS, &peidl); SUCCEEDED(enumHr)) {
        PITEMID_CHILD pidlItem = nullptr;
        while (peidl->Next(1, &pidlItem, nullptr) == S_OK) {
            CComPtr<IShellItem> pItem;
            if (const auto createItemHr = SHCreateItemFromIDList(pidlItem, IID_PPV_ARGS(&pItem)); FAILED(createItemHr)) {
                CoTaskMemFree(pidlItem);
                continue;
            }

            STRRET strRet;
            ZeroMemory(&strRet, sizeof(strRet));
            strRet.uType = STRRET_WSTR;

            if (const auto displayNameHr =
                        folder->GetDisplayNameOf(pidlItem, SHGDN_FORADDRESSBAR | SHGDN_INFOLDER | SHGDN_FOREDITING, &strRet);
                FAILED(displayNameHr)) {
                CoTaskMemFree(pidlItem);
                continue;
            }

            LPTSTR lptstr = nullptr;
            if (const auto stringReturnToStrHr = StrRetToStr(&strRet, pidlItem, &lptstr); FAILED(stringReturnToStrHr)) {
                CoTaskMemFree(pidlItem);
                CoTaskMemFree(lptstr);
                continue;
            }

            const bool match = SyncPath(lptstr) == SyncPath(relativePath.front()).stem();
            if (relativePath.size() == 1 && match) {
                return true;
                CoTaskMemFree(pidlItem);
                CoTaskMemFree(lptstr);
                break;
            }

            if (match && isFolder(pItem)) {
                auto childRelativedPath = relativePath;
                childRelativedPath.pop_front();
                IShellFolder2 *psChildFolder = nullptr;
                (void) SHBindToObject(folder, pidlItem, nullptr, IID_IShellFolder2, reinterpret_cast<void **>(&psChildFolder));

                return isInFolder(childRelativedPath, psChildFolder);
            }
        }
    }

    if (peidl) (void) peidl->Release();

    return false;
}

bool isInFolder(const SyncPath &relativePath, IShellFolder2 *folder) {
    return isInFolder(CommonUtility::splitSyncPath(relativePath), folder);
}

} // namespace

bool Utility::isInTrash(const SyncPath &relativePath) {
    bool found = false;

    if (const auto coInitializeHr = CoInitialize(nullptr); SUCCEEDED(coInitializeHr)) {
        IShellFolder2 *psfRecycleBin = nullptr;
        if (const auto bindingHr = bindToCsidl(CSIDL_BITBUCKET, IID_PPV_ARGS(&psfRecycleBin)); SUCCEEDED(bindingHr)) {
            found = isInFolder(relativePath, psfRecycleBin);
            (void) psfRecycleBin->Release();
        }
        CoUninitialize();
    }

    return found;
}

} // namespace KDC
