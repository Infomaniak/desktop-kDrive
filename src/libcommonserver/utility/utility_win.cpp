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

#include "libcommon/utility/utility.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/io/iohelper_win.h"
#include "log/log.h"

#include <filesystem>
#include <string>
#include <iostream>
#include <system_error>

// TODO: check which includes are actually necessary
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
    IShellItem *fileOrFolderItem = NULL;
    hr = SHCreateItemFromParsingName(itemPathPreferred.make_preferred().native().c_str(), NULL, IID_PPV_ARGS(&fileOrFolderItem));
    if (FAILED(hr)) {
        // Couldn't get file into an item - clean up and return (maybe the file doesn't exist?)
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

    hr = fileOperation->DeleteItem(fileOrFolderItem, NULL);
    if (FAILED(hr)) {
        // Failed to mark file/folder item for deletion - clean up and return
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
    return CommonUtility::ws2s(std::wstring(userName));
}

} // namespace KDC
