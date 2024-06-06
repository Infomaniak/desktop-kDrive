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

#include "common/filesystembase.h"

#include "libcommonserver/io/filestat.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/io/iohelper_win.h"

#include "libcommonserver/utility/utility.h"  // Path2WStr
#include "libcommon/utility/utility.h"

#include "log/log.h"

#include <log4cplus/loggingmacros.h>

#include <filesystem>
#include <string>

#include <windows.h>
#include <Shobjidl.h>  //Required for IFileOperation Interface
#include <shellapi.h>  //Required for Flags set in "SetOperationFlags"
#include <objbase.h>
#include <objidl.h>
#include <shlguid.h>
#include <strsafe.h>
#include <AclAPI.h>
#include <AccCtrl.h>
#define SECURITY_WIN32
#include <security.h>
#include <ntstatus.h>

constexpr int MAX_GET_RIGHTS_DURATION_MS = 60;
namespace KDC {

namespace {
IoError dWordError2ioError(DWORD error) noexcept {
    switch (error) {
        case ERROR_SUCCESS:
            return IoErrorSuccess;
        case ERROR_ACCESS_DENIED:
            return IoErrorAccessDenied;
        case ERROR_DISK_FULL:
            return IoErrorDiskFull;
        case ERROR_ALREADY_EXISTS:
            return IoErrorFileExists;
        case ERROR_INVALID_PARAMETER:
            return IoErrorInvalidArgument;
        case ERROR_FILENAME_EXCED_RANGE:
            return IoErrorFileNameTooLong;
        case ERROR_FILE_NOT_FOUND:
        case ERROR_INVALID_DRIVE:
        case ERROR_PATH_NOT_FOUND:
        case ERROR_INVALID_NAME:
            return IoErrorNoSuchFileOrDirectory;
        default:
            LOG_WARN(Log::instance()->getLogger(), L"Unknown IO error - error=" << error);
            return IoErrorUnknown;
    }
}

IoError ntStatus2ioError(NTSTATUS status) noexcept {
    switch (status) {
        case STATUS_SUCCESS:
            return IoErrorSuccess;
        case STATUS_ACCESS_DENIED:
            return IoErrorAccessDenied;
        case STATUS_DISK_FULL:
            return IoErrorDiskFull;
        case STATUS_INVALID_PARAMETER:
            return IoErrorInvalidArgument;
        case STATUS_NO_SUCH_FILE:
        case STATUS_NO_SUCH_DEVICE:
            return IoErrorNoSuchFileOrDirectory;
        default:
            return IoErrorUnknown;
    }
}

time_t FileTimeToUnixTime(FILETIME *filetime, DWORD *remainder) {
    long long int t = filetime->dwHighDateTime;
    t <<= 32;
    t += (UINT32)filetime->dwLowDateTime;
    t -= 116444736000000000LL;
    if (t < 0) {
        if (remainder) *remainder = 9999999 - (-t - 1) % 10000000;
        return -1 - ((-t - 1) / 10000000);
    } else {
        if (remainder) *remainder = t % 10000000;
        return t / 10000000;
    }
}

time_t FileTimeToUnixTime(LARGE_INTEGER filetime, DWORD *remainder) {
    FILETIME ft;
    ft.dwHighDateTime = filetime.HighPart;
    ft.dwLowDateTime = filetime.LowPart;
    return FileTimeToUnixTime(&ft, remainder);
}
}  // namespace

int IoHelper::_getAndSetRightsMethod = -1;  // -1: not initialized, 0: Windows API, 1: std::filesystem
bool IoHelper::_setRightsWindowsApiInheritance = false;
std::unique_ptr<BYTE[]> IoHelper::_psid = nullptr;
TRUSTEE IoHelper::_trustee = {nullptr};
std::mutex IoHelper::_initRightsWindowsApiMutex;

bool IoHelper::fileExists(const std::error_code &ec) noexcept {
    return (ec.value() != ERROR_FILE_NOT_FOUND) && (ec.value() != ERROR_PATH_NOT_FOUND) && (ec.value() != ERROR_INVALID_DRIVE);
}

IoError IoHelper::stdError2ioError(int error) noexcept {
    return dWordError2ioError(static_cast<DWORD>(error));
}

bool IoHelper::getNodeId(const SyncPath &path, NodeId &nodeId) noexcept {
    // Get parent folder handle
    HANDLE hParent;
    hParent = CreateFileW(path.parent_path().wstring().c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                          FILE_FLAG_BACKUP_SEMANTICS, NULL);

    if (hParent == INVALID_HANDLE_VALUE) {
        LOGW_INFO(Log::instance()->getLogger(), L"Error in CreateFileW: " << Utility::formatSyncPath(path.parent_path()).c_str());
        return false;
    }

    // Get file information
    WCHAR szFileName[MAX_PATH_LENGTH_WIN_LONG];
    lstrcpyW(szFileName, path.filename().wstring().c_str());

    UNICODE_STRING fn;
    fn.Buffer = (LPWSTR)szFileName;
    fn.Length = (unsigned short)lstrlen(szFileName) * sizeof(WCHAR);
    fn.MaximumLength = sizeof(szFileName);

    IO_STATUS_BLOCK iosb;
    RtlZeroMemory((PVOID)&iosb, sizeof(iosb));

    LONGLONG fileInfo[(MAX_PATH_LENGTH_WIN_LONG + sizeof(FILE_ID_FULL_DIR_INFORMATION)) / sizeof(LONGLONG)];
    PFILE_ID_FULL_DIR_INFORMATION pFileInfo = (PFILE_ID_FULL_DIR_INFORMATION)fileInfo;

    PZW_QUERY_DIRECTORY_FILE zwQueryDirectoryFile =
        (PZW_QUERY_DIRECTORY_FILE)GetProcAddress(GetModuleHandle(L"ntdll.dll"), "ZwQueryDirectoryFile");

    if (zwQueryDirectoryFile == 0) {
        LOG_WARN(Log::instance()->getLogger(),
                 L"Error in GetProcAddress: " << Utility::formatSyncPath(path.parent_path()).c_str());
        return false;
    }

    NTSTATUS status = zwQueryDirectoryFile(hParent, NULL, NULL, NULL, &iosb, fileInfo, sizeof(fileInfo),
                                           FileIdFullDirectoryInformation, true, &fn, TRUE);

    if (!NT_SUCCESS(status)) {
        CloseHandle(hParent);
        return false;
    }

    // Get the Windows file id as an inode replacement.
    nodeId = std::to_string(((long long)pFileInfo->FileId.HighPart << 32) + (long long)pFileInfo->FileId.LowPart);

    CloseHandle(hParent);
    return true;
}

bool IoHelper::getFileStat(const SyncPath &path, FileStat *buf, IoError &ioError) noexcept {
    ioError = IoErrorSuccess;

    // Get parent folder handle
    HANDLE hParent;
    bool retry = true;
    int counter = 50;
    while (retry) {
        retry = false;

        hParent = CreateFileW(path.parent_path().wstring().c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                              FILE_FLAG_BACKUP_SEMANTICS, NULL);
        if (hParent == INVALID_HANDLE_VALUE) {
            DWORD dwError = GetLastError();
            if (counter) {
                retry = true;
                Utility::msleep(10);
                LOGW_DEBUG(Log::instance()->getLogger(),
                           L"Retrying to get handle: " << Utility::formatSyncPath(path.parent_path()).c_str());
                counter--;
                continue;
            }

            LOG_WARN(logger(), L"Error in CreateFileW: " << Utility::formatSyncPath(path.parent_path()).c_str());
            ioError = dWordError2ioError(dwError);
            return _isExpectedError(ioError);
        }
    }

    // Get file information
    WCHAR szFileName[MAX_PATH_LENGTH_WIN_LONG];
    StringCchCopy(szFileName, MAX_PATH_LENGTH_WIN_LONG, path.filename().wstring().c_str());

    UNICODE_STRING fn;
    fn.Buffer = (LPWSTR)szFileName;
    fn.Length = (unsigned short)lstrlen(szFileName) * sizeof(WCHAR);
    fn.MaximumLength = sizeof(szFileName);

    IO_STATUS_BLOCK iosb;
    RtlZeroMemory((PVOID)&iosb, sizeof(iosb));

    LONGLONG fileInfo[(MAX_PATH_LENGTH_WIN_LONG + sizeof(FILE_ID_FULL_DIR_INFORMATION)) / sizeof(LONGLONG)];
    PFILE_ID_FULL_DIR_INFORMATION pFileInfo = (PFILE_ID_FULL_DIR_INFORMATION)fileInfo;

    PZW_QUERY_DIRECTORY_FILE zwQueryDirectoryFile =
        (PZW_QUERY_DIRECTORY_FILE)GetProcAddress(GetModuleHandle(L"ntdll.dll"), "ZwQueryDirectoryFile");

    if (zwQueryDirectoryFile == 0) {
        LOG_WARN(Log::instance()->getLogger(), "Error in GetProcAddress");
        ioError = dWordError2ioError(GetLastError());
        CloseHandle(hParent);
        return false;
    }

    NTSTATUS status = zwQueryDirectoryFile(hParent, NULL, NULL, NULL, &iosb, fileInfo, sizeof(fileInfo),
                                           FileIdFullDirectoryInformation, true, &fn, TRUE);

    DWORD dwError = GetLastError();
    const bool isNtfs = Utility::isNtfs(path);
    if ((isNtfs && !NT_SUCCESS(status)) ||
        (!isNtfs && dwError != 0)) {  // On FAT32 file system, NT_SUCCESS will return false even if it is a success, therefore we
                                      // also check GetLastError
        LOGW_DEBUG(Log::instance()->getLogger(),
                   L"Error in zwQueryDirectoryFile: " << Utility::formatSyncPath(path.parent_path()).c_str());
        CloseHandle(hParent);

        if (!NT_SUCCESS(status)) {
            ioError = ntStatus2ioError(status);
        } else if (dwError != 0) {
            ioError = dWordError2ioError(dwError);
        }
        return _isExpectedError(ioError);
    }

    // Get the Windows file id as an inode replacement.
    buf->inode = ((long long)pFileInfo->FileId.HighPart << 32) + (long long)pFileInfo->FileId.LowPart;
    buf->size = ((long long)pFileInfo->EndOfFile.HighPart << 32) + (long long)pFileInfo->EndOfFile.LowPart;

    DWORD rem;
    buf->modtime = FileTimeToUnixTime(pFileInfo->LastWriteTime, &rem);
    buf->creationTime = FileTimeToUnixTime(pFileInfo->CreationTime, &rem);

    buf->isHidden = pFileInfo->FileAttributes & FILE_ATTRIBUTE_HIDDEN;
    buf->nodeType = pFileInfo->FileAttributes & FILE_ATTRIBUTE_DIRECTORY ? NodeTypeDirectory : NodeTypeFile;
    CloseHandle(hParent);

    return true;
}

bool IoHelper::isFileAccessible(const SyncPath &absolutePath, IoError &ioError) {
    ioError = IoErrorSuccess;

    HANDLE hFile = CreateFileW(Path2WStr(absolutePath).c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, NULL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        ioError = dWordError2ioError(GetLastError());
        return false;
    }

    CloseHandle(hFile);

    return true;
}

bool IoHelper::_checkIfIsHiddenFile(const SyncPath &filepath, bool &isHidden, IoError &ioError) noexcept {
    isHidden = false;
    ioError = IoErrorSuccess;

    DWORD result = GetFileAttributes(filepath.c_str());
    if (result != INVALID_FILE_ATTRIBUTES) {
        isHidden = !!(result & FILE_ATTRIBUTE_HIDDEN);
        return true;
    } else {
        ioError = dWordError2ioError(GetLastError());
        return _isExpectedError(ioError);
    }
}

void IoHelper::setFileHidden(const SyncPath &path, bool hidden) noexcept {
    // TODO: move KDC::FileSystem::longWinPath to IoHelper and use it here.
    DWORD dwAttrs;

    const wchar_t *fName = path.c_str();
    dwAttrs = GetFileAttributesW(fName);

    if (dwAttrs != INVALID_FILE_ATTRIBUTES) {
        if (hidden && !(dwAttrs & FILE_ATTRIBUTE_HIDDEN)) {
            SetFileAttributesW(fName, dwAttrs | FILE_ATTRIBUTE_HIDDEN);
        } else if (!hidden && (dwAttrs & FILE_ATTRIBUTE_HIDDEN)) {
            SetFileAttributesW(fName, dwAttrs & ~FILE_ATTRIBUTE_HIDDEN);
        }
    }
}

bool IoHelper::setXAttrValue(const SyncPath &path, DWORD attrCode, IoError &ioError) noexcept {
    ioError = IoErrorSuccess;
    bool result = SetFileAttributesW(path.c_str(), attrCode);

    if (!result) {
        ioError = dWordError2ioError(GetLastError());
        return _isExpectedError(ioError);
    }

    return true;
}

bool IoHelper::getXAttrValue(const SyncPath &path, DWORD attrCode, bool &value, IoError &ioError) noexcept {
    value = false;
    ioError = IoErrorSuccess;
    DWORD result = GetFileAttributesW(path.c_str());

    if (result == INVALID_FILE_ATTRIBUTES) {
        ioError = dWordError2ioError(GetLastError());

        return _isExpectedError(ioError);
    }

    // XAttr has been read
    value = result & attrCode;

    return true;
}

bool IoHelper::checkIfFileIsDehydrated(const SyncPath &itemPath, bool &isDehydrated, IoError &ioError) noexcept {
    isDehydrated = false;
    ioError = IoErrorSuccess;

    return IoHelper::getXAttrValue(itemPath.native(), FILE_ATTRIBUTE_OFFLINE, isDehydrated, ioError);
}


TRUSTEE &IoHelper::getTrustee() {
    return _trustee;
}

void IoHelper::initRightsWindowsApi() {
    std::lock_guard lock(_initRightsWindowsApiMutex);
    if (_getAndSetRightsMethod != -1) {
        return;
    }
    _getAndSetRightsMethod = 1;  // Fallback method by default
    _trustee = {nullptr};
    _psid = nullptr;

    if (const std::string useGetRightsFallbackMethod = CommonUtility::envVarValue("KDRIVE_USE_GETRIGHTS_FALLBACK_METHOD");
        !useGetRightsFallbackMethod.empty()) {
        LOG_DEBUG(Log::instance()->getLogger(), "KDRIVE_USE_GETRIGHTS_FALLBACK_METHOD env is set, using fallback method");
        return;
    }

    // Get SID associated with the current process
    auto hToken_std = std::make_unique<HANDLE>();
    PHANDLE hToken = hToken_std.get();
    if (!OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, hToken)) {
        DWORD dwError = GetLastError();
        if (dwError == ERROR_NO_TOKEN) {
            if (!ImpersonateSelf(SecurityImpersonation)) {
                dwError = GetLastError();
                LOGW_WARN(Log::instance()->getLogger(), "Error in ImpersonateSelf - err=" << dwError);
                return;
            }

            if (!OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, hToken)) {
                dwError = GetLastError();
                LOGW_WARN(Log::instance()->getLogger(), "Error in OpenThreadToken - err=" << dwError);
                return;
            }
        } else {
            LOGW_WARN(Log::instance()->getLogger(), "Error in OpenThreadToken - err=" << dwError);
            return;
        }
    }

    DWORD dwLength = 0;
    GetTokenInformation(*hToken, TokenUser, nullptr, 0, &dwLength);
    if (dwLength == 0) {
        DWORD dwError = GetLastError();
        LOGW_WARN(Log::instance()->getLogger(), "Error in GetTokenInformation 1 - err=" << dwError);
        return;
    }
    auto pTokenUser_std = std::make_unique<TOKEN_USER[]>(dwLength);
    PTOKEN_USER pTokenUser = pTokenUser_std.get();
    if (pTokenUser == nullptr) {
        LOGW_WARN(Log::instance()->getLogger(), "Memory allocation error");
        return;
    }

    if (!GetTokenInformation(*hToken, TokenUser, pTokenUser, dwLength, &dwLength)) {
        DWORD dwError = GetLastError();
        LOGW_WARN(Log::instance()->getLogger(), "Error in GetTokenInformation 2 - err=" << dwError);
        return;
    }

    _psid = std::make_unique<BYTE[]>(GetLengthSid(pTokenUser->User.Sid));

    if (!CopySid(GetLengthSid(pTokenUser->User.Sid), _psid.get(), pTokenUser->User.Sid)) {
        DWORD dwError = GetLastError();
        LOGW_WARN(Log::instance()->getLogger(), "Error in CopySid - err=" << dwError);
        _psid.reset();
        return;
    }

    // initialize the trustee structure
    BuildTrusteeWithSid(&_trustee, _psid.get());
    _getAndSetRightsMethod = 0;  // Windows API method

    // Check getRights method performance
    SyncPath tmpDir;
    IoError ioError = IoErrorSuccess;
    if (!IoHelper::tempDirectoryPath(tmpDir, ioError)) {
        LOGW_WARN(Log::instance()->getLogger(),
                  "Error in IoHelper::tempDirectoryPath: " << Utility::formatIoError(tmpDir, ioError));
        return;
    }

    bool read = true;
    bool write = true;
    bool execute = true;

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 10; i++) {
        if (!IoHelper::getRights(tmpDir, read, write, execute, ioError)) {
            LOGW_WARN(Log::instance()->getLogger(), "Error in IoHelper::getRights: " << Utility::formatIoError(tmpDir, ioError));
            _getAndSetRightsMethod = 1;  // Fallback method
            return;
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    /* Average time spent in getRights with windows API:
     *    -> Windows 11 without Active Directory: 1ms < time < 4ms
     *    -> Windows 11 with Active Directory: 70ms < time < 110ms
     */
    LOG_DEBUG(Log::instance()->getLogger(), "getRights duration: " << double(duration / 1000.0) << "ms");

    if (double(duration / 1000.0) > MAX_GET_RIGHTS_DURATION_MS) {
        LOG_WARN(Log::instance()->getLogger(), "Get/Set rights using windows API is too slow to be used. Using fallback method.");
        _getAndSetRightsMethod = 1;  // Fallback method
        return;
    }
}

// Always return false if ioError != IoErrorSuccess, caller should call _isExpectedError
static bool setRightsWindowsApi(const SyncPath &path, DWORD permission, ACCESS_MODE accessMode, IoError &ioError,
                                log4cplus::Logger logger, bool inherite = false) noexcept {
    PACL pACLold = nullptr;  // Current ACL
    PACL pACLnew = nullptr;  // New ACL
    PSECURITY_DESCRIPTOR pSecurityDescriptor = nullptr;
    EXPLICIT_ACCESS explicitAccess;
    ZeroMemory(&explicitAccess, sizeof(explicitAccess));

    explicitAccess.grfAccessPermissions = permission;
    explicitAccess.grfAccessMode = accessMode;
    if (!inherite) {
        explicitAccess.grfInheritance = NO_INHERITANCE;
    } else {
        explicitAccess.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
    }
    explicitAccess.Trustee.pMultipleTrustee = nullptr;
    explicitAccess.Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
    explicitAccess.Trustee.TrusteeForm = IoHelper::getTrustee().TrusteeForm;
    explicitAccess.Trustee.TrusteeType = IoHelper::getTrustee().TrusteeType;
    explicitAccess.Trustee.ptstrName = IoHelper::getTrustee().ptstrName;

    std::wstring pathWstr = Path2WStr(path);
    size_t pathLen = pathWstr.length();

    auto pathwPtr = std::make_unique<WCHAR[]>(pathLen + 1);
    pathWstr.copy(pathwPtr.get(), pathLen);

    LPCWSTR pathw_c = pathWstr.c_str();
    LPWSTR pathw = pathwPtr.get();
    pathw[pathLen] = L'\0';

    DWORD result = GetNamedSecurityInfo(pathw_c, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, &pACLold, nullptr,
                                        &pSecurityDescriptor);
    ioError = dWordError2ioError(result);
    if (result != ERROR_SUCCESS) {
        ioError = dWordError2ioError(result);
        LOGW_WARN(logger, L"Error in GetNamedSecurityInfo: path='" << Utility::formatSyncPath(path) << L"',DWORD err='" << result
                                                                   << L"'");
        LocalFree(pSecurityDescriptor);
        LocalFree(pACLnew);
        // pACLold is a pointer to the ACL in the security descriptor, so it should not be freed.
        return false;
    }

    result = SetEntriesInAcl(1, &explicitAccess, pACLold, &pACLnew);
    ioError = dWordError2ioError(result);
    if (result != ERROR_SUCCESS) {
        ioError = dWordError2ioError(result);
        LOGW_WARN(logger,
                  L"Error in SetEntriesInAcl: path='" << Utility::formatSyncPath(path) << L"',DWORD err='" << result << L"'");
        LocalFree(pSecurityDescriptor);
        LocalFree(pACLnew);
        // pACLold is a pointer to the ACL in the security descriptor, so it should not be freed.
        return false;
    }

    if (!IsValidAcl(pACLnew)) {
        ioError = IoErrorUnknown;
        LOGW_WARN(logger, L"Invalid new ACL: " << Utility::formatSyncPath(path).c_str());

        LocalFree(pSecurityDescriptor);
        LocalFree(pACLnew);
        // pACLold is a pointer to the ACL in the security descriptor, so it should not be freed.
        return false;
    }

    result = SetNamedSecurityInfo(pathw, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, pACLnew, nullptr);
    ioError = dWordError2ioError(result);
    if (result != ERROR_SUCCESS) {
        ioError = dWordError2ioError(result);
        LOGW_WARN(logger, L"Error in SetNamedSecurityInfo: path='" << Utility::formatSyncPath(path) << L"',DWORD err='" << result
                                                                   << L"'");
        LocalFree(pSecurityDescriptor);
        LocalFree(pACLnew);
        // pACLold is a pointer to the ACL in the security descriptor, so it should not be freed.
        return false;
    }

    LocalFree(pSecurityDescriptor);
    LocalFree(pACLnew);
    // pACLold is a pointer to the ACL in the security descriptor, so it should not be freed.
    return true;
}

// Always return false if ioError != IoErrorSuccess, caller should call _isExpectedError.
static bool getRightsWindowsApi(const SyncPath &path, bool &read, bool &write, bool &exec, IoError &ioError,
                                log4cplus::Logger logger, bool retry = true) noexcept {
    ioError = IoErrorSuccess;
    read = false;
    write = false;
    exec = false;

    WCHAR szFilePath[MAX_PATH_LENGTH_WIN_LONG];
    lstrcpyW(szFilePath, path.native().c_str());

    // Get security info
    PACL pfileACL = nullptr;
    PSECURITY_DESCRIPTOR psecDesc = nullptr;
    DWORD result = GetNamedSecurityInfo(szFilePath, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, &pfileACL,
                                        nullptr, &psecDesc);

    ioError = dWordError2ioError(result);
    if (ioError != IoErrorSuccess) {
        LOGW_WARN(logger,
                  L"GetNamedSecurityInfo failed: path='" << Utility::formatSyncPath(path) << L"',DWORD err='" << result << L"'");
        LocalFree(psecDesc);
        return false;  // Caller should call _isExpectedError
    }

    // Get rights for trustee
    ACCESS_MASK rights = 0;
    result = GetEffectiveRightsFromAcl(pfileACL, &IoHelper::getTrustee(), &rights);
    ioError = dWordError2ioError(result);

    /* The GetEffectiveRightsFromAcl function fails and returns ERROR_INVALID_ACL if the specified ACL contains an inherited
     * access-denied ACE. see: https://learn.microsoft.com/en-us/windows/win32/api/aclapi/nf-aclapi-geteffectiverightsfromacla
     * From my personal test on Windows 11 Pro 23H2, the function does work as expected and returns the rights for the trustee
     * even if the ACL contains an inherited access-denied ACE. If we get ERROR_INVALID_ACL, we will consider to be in the case
     * described in the documentation and consider the file as not existing (we can't get the rights).
     */
    if (result == ERROR_INVALID_ACL) {
        LOGW_INFO(
            logger,
            L"getRightsWindowsApi: path='"
                << Utility::formatSyncPath(path)
                << L"', the specified ACL contains an inherited access - denied ACE. Considerring the file as not existing.");
        read = false;
        write = false;
        exec = false;
        ioError = IoErrorNoSuchFileOrDirectory;
        LocalFree(psecDesc);
        return true;
    }

    if (result != ERROR_SUCCESS) {
        LOGW_WARN(logger, L"GetEffectiveRightsFromAcl failed: path='" << Utility::formatSyncPath(path) << L"',DWORD err='"
                                                                      << result << L"'");
        LocalFree(psecDesc);
        return false;  // Caller should call _isExpectedError
    }

    LocalFree(psecDesc);
    read = (rights & FILE_GENERIC_READ) == FILE_GENERIC_READ;
    write = (rights & FILE_GENERIC_WRITE) == FILE_GENERIC_WRITE;
    exec = (rights & FILE_EXECUTE) == FILE_EXECUTE;
    return true;
}

bool IoHelper::getRights(const SyncPath &path, bool &read, bool &write, bool &exec, IoError &ioError) noexcept {
    // !!! When Full control to file/directory is denied to the user, the function will return as if the file/directory does not exist.
    read = false;
    write = false;
    exec = false;
    if (_getAndSetRightsMethod == -1) initRightsWindowsApi();
    // Preferred method
    if (_getAndSetRightsMethod == 0 && IoHelper::getTrustee().ptstrName) {
        if (getRightsWindowsApi(path, read, write, exec, ioError, logger()) || _isExpectedError(ioError)) {
            return true;
        }
        LOGW_WARN(logger(), L"Failed to get rights using Windows API, falling back to std::filesystem.");
        sentry_value_t event = sentry_value_new_event();
        sentry_value_t exc = sentry_value_new_exception(
            "Exception", "Failed to set/get rights using Windows API, falling back to std::filesystem.");
        sentry_value_set_stacktrace(exc, NULL, 0);
        sentry_event_add_exception(event, exc);
        sentry_capture_event(event);
        IoHelper::getTrustee().ptstrName = nullptr;
        _getAndSetRightsMethod = 1;
    }
    // Fallback method.
    ItemType itemType;
    const bool success = getItemType(path, itemType);
    ioError = itemType.ioError;

    if (!success) {
        LOGW_WARN(logger(), L"Failed to get item type: " << Utility::formatIoError(path, ioError).c_str());
        return false;
    }

    if (ioError != IoErrorSuccess) {
        return _isExpectedError(ioError);
    }
    const bool isSymlink = itemType.linkType == LinkTypeSymlink;

    std::error_code ec;
    std::filesystem::perms perms =
        isSymlink ? std::filesystem::symlink_status(path, ec).permissions() : std::filesystem::status(path, ec).permissions();
    ioError = stdError2ioError(ec);
    if (ioError != IoErrorSuccess) {
        LOGW_WARN(logger(), L"Failed to get permissions: " << Utility::formatStdError(path, ec).c_str());
        return _isExpectedError(ioError);
    }
    read = ((perms & std::filesystem::perms::owner_read) != std::filesystem::perms::none);
    write = ((perms & std::filesystem::perms::owner_write) != std::filesystem::perms::none);
    exec = ((perms & std::filesystem::perms::owner_exec) != std::filesystem::perms::none);
    return true;
}

bool IoHelper::setRights(const SyncPath &path, bool read, bool write, bool exec, IoError &ioError) noexcept {
    if (_getAndSetRightsMethod == -1) initRightsWindowsApi();
    // Preferred method
    if (_getAndSetRightsMethod == 0 && IoHelper::getTrustee().ptstrName) {
        ioError = IoErrorSuccess;
        DWORD grantedPermission = WRITE_DAC;
        DWORD deniedPermission = 0;

        if (!read) {
            deniedPermission |= (FILE_GENERIC_READ & ~READ_CONTROL & ~SYNCHRONIZE);
        } else {
            grantedPermission |= FILE_GENERIC_READ;
        }

        if (!write) {
            deniedPermission |= (FILE_GENERIC_WRITE & ~READ_CONTROL & ~SYNCHRONIZE);
        } else {
            grantedPermission |= FILE_GENERIC_WRITE;
        }

        if (!exec) {
            deniedPermission |= (FILE_GENERIC_EXECUTE & ~READ_CONTROL & ~SYNCHRONIZE & ~FILE_READ_ATTRIBUTES);
        } else {
            grantedPermission |= FILE_GENERIC_EXECUTE;
        }
        // clang-format off
        bool res = setRightsWindowsApi(path, grantedPermission, ACCESS_MODE::SET_ACCESS, ioError, logger(),
                                       _setRightsWindowsApiInheritance) 
            || _isExpectedError(ioError);
        if (res) {
            res &= setRightsWindowsApi(path, deniedPermission, ACCESS_MODE::DENY_ACCESS, ioError, logger(),
                                       _setRightsWindowsApiInheritance) 
                || _isExpectedError(ioError);
        }
        // clang-format on

        if (res) {
            return true;
        }

        LOGW_WARN(logger(), L"Failed to set rights using Windows API, falling back to std::filesystem.");
        sentry_value_t event = sentry_value_new_event();
        sentry_value_t exc = sentry_value_new_exception(
            "Exception", "Failed to set/get rights using Windows API, falling back to std::filesystem.");
        sentry_value_set_stacktrace(exc, NULL, 0);
        sentry_event_add_exception(event, exc);
        sentry_capture_event(event);
        IoHelper::getTrustee().ptstrName = nullptr;
        _getAndSetRightsMethod = 1;
    }
    return _setRightsStd(path, read, write, exec, ioError);
}

bool IoHelper::checkIfIsJunction(const SyncPath &path, bool &isJunction, IoError &ioError) noexcept {
    isJunction = false;
    ioError = IoErrorSuccess;

    HANDLE hFile =
        CreateFileW(Path2WStr(path).c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                    OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        ioError = dWordError2ioError(GetLastError());
        return _isExpectedError(ioError);
    }

    BYTE buf[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];
    REPARSE_DATA_BUFFER &ReparseBuffer = (REPARSE_DATA_BUFFER &)buf;
    DWORD dwRet;
    if (!DeviceIoControl(hFile, FSCTL_GET_REPARSE_POINT, NULL, 0, &ReparseBuffer, MAXIMUM_REPARSE_DATA_BUFFER_SIZE, &dwRet,
                         NULL)) {
        DWORD dwError = GetLastError();
        CloseHandle(hFile);

        const bool notReparsePoint = (dwError == ERROR_NOT_A_REPARSE_POINT);
        if (notReparsePoint) {
            return true;
        } else {
            ioError = dWordError2ioError(dwError);
            return _isExpectedError(ioError);
        }
    }
    CloseHandle(hFile);
    isJunction = ReparseBuffer.ReparseTag == IO_REPARSE_TAG_MOUNT_POINT;
    return true;
}

bool IoHelper::createJunction(const std::string &data, const SyncPath &path, IoError &ioError) noexcept {
    ioError = IoErrorSuccess;

    // Create the junction directory
    if (!CreateDirectoryW(Path2WStr(path).c_str(), NULL)) {
        ioError = dWordError2ioError(GetLastError());
        return false;
    }

    // Set the reparse point
    HANDLE hDir;
    hDir = CreateFileW(Path2WStr(path).c_str(), GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                       FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (hDir == INVALID_HANDLE_VALUE) {
        ioError = dWordError2ioError(GetLastError());
        return false;
    }

    DWORD dwRet = 0;
    if (!DeviceIoControl(hDir, FSCTL_SET_REPARSE_POINT, (void *)data.data(), (WORD)data.size(), NULL, 0, &dwRet, NULL)) {
        RemoveDirectoryW(Path2WStr(path).c_str());
        CloseHandle(hDir);
        ioError = dWordError2ioError(GetLastError());
        return false;
    }

    CloseHandle(hDir);
    return true;
}

bool IoHelper::readJunction(const SyncPath &path, std::string &data, SyncPath &targetPath, IoError &ioError) noexcept {
    ioError = IoErrorSuccess;

    HANDLE hFile =
        CreateFileW(Path2WStr(path).c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                    OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        ioError = dWordError2ioError(GetLastError());
        return _isExpectedError(ioError);
    }

    BYTE buf[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];
    REPARSE_DATA_BUFFER &reparseBuffer = (REPARSE_DATA_BUFFER &)buf;
    DWORD dwRet;
    if (!DeviceIoControl(hFile, FSCTL_GET_REPARSE_POINT, NULL, 0, &reparseBuffer, MAXIMUM_REPARSE_DATA_BUFFER_SIZE, &dwRet,
                         NULL)) {
        DWORD dwError = GetLastError();
        CloseHandle(hFile);

        ioError = dWordError2ioError(dwError);
        return _isExpectedError(ioError);
    }

    CloseHandle(hFile);

    data = std::string(reinterpret_cast<char const *>(buf), dwRet);

    WORD nameLength = reparseBuffer.MountPointReparseBuffer.SubstituteNameLength / sizeof(TCHAR);
    WORD nameOffset = reparseBuffer.MountPointReparseBuffer.SubstituteNameOffset / sizeof(TCHAR);

    LPWSTR pPath = reparseBuffer.MountPointReparseBuffer.PathBuffer + nameOffset;
    pPath[nameLength] = L'\0';

    if (wcsncmp(pPath, L"\\??\\", 4) == 0) pPath += 4;  // Skip 'non-parsed' prefix
    targetPath = SyncPath(pPath);

    return true;
}

bool IoHelper::createJunctionFromPath(const SyncPath &targetPath, const SyncPath &path, IoError &ioError) noexcept {
    ioError = IoErrorSuccess;

    // Create the junction directory
    if (!CreateDirectoryW(Path2WStr(path).c_str(), NULL)) {
        ioError = dWordError2ioError(GetLastError());
        return false;
    }

    // Set the reparse point
    HANDLE hDir;
    hDir = CreateFileW(Path2WStr(path).c_str(), GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                       FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (hDir == INVALID_HANDLE_VALUE) {
        ioError = dWordError2ioError(GetLastError());
        return false;
    }

    const std::wstring prefixedTargetPath = L"\\??\\" + Path2WStr(targetPath);
    auto const targetPathCstr = prefixedTargetPath.c_str();
    int64_t targetPathWLen = wcslen(targetPathCstr);
    if (targetPathWLen > MAX_PATH - 1) {
        CloseHandle(hDir);
        ioError = IoErrorFileNameTooLong;
        return false;
    }

    const int reparseDataBufferSize = sizeof(REPARSE_DATA_BUFFER) + 2 * MAX_PATH * sizeof(WCHAR);
    REPARSE_DATA_BUFFER *reparseDataBuffer = static_cast<REPARSE_DATA_BUFFER *>(calloc(reparseDataBufferSize, 1));

    reparseDataBuffer->ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
    wcscpy(reparseDataBuffer->MountPointReparseBuffer.PathBuffer, targetPathCstr);
    wcscpy(reparseDataBuffer->MountPointReparseBuffer.PathBuffer + targetPathWLen + 1, targetPathCstr);
    reparseDataBuffer->MountPointReparseBuffer.SubstituteNameOffset = 0;
    reparseDataBuffer->MountPointReparseBuffer.SubstituteNameLength = static_cast<WORD>(targetPathWLen * sizeof(WCHAR));
    reparseDataBuffer->MountPointReparseBuffer.PrintNameOffset = static_cast<WORD>((targetPathWLen + 1) * sizeof(WCHAR));
    reparseDataBuffer->MountPointReparseBuffer.PrintNameLength = static_cast<WORD>(targetPathWLen * sizeof(WCHAR));
    reparseDataBuffer->ReparseDataLength =
        static_cast<WORD>((targetPathWLen + 1) * 2 * sizeof(WCHAR) + REPARSE_MOUNTPOINT_HEADER_SIZE);

    DWORD dwError = ERROR_SUCCESS;
    const bool success =
        DeviceIoControl(hDir, FSCTL_SET_REPARSE_POINT, reparseDataBuffer,
                        reparseDataBuffer->ReparseDataLength + REPARSE_MOUNTPOINT_HEADER_SIZE, NULL, 0, &dwError, NULL);

    ioError = dWordError2ioError(dwError);
    free(reparseDataBuffer);

    if (CloseHandle(hDir) == 0) {
        ioError = dWordError2ioError(GetLastError());
        return false;
    }

    return success;
}

}  // namespace KDC
