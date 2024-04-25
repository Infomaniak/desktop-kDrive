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
#include <iostream>

namespace KDC {

namespace {
IoError dWordError2ioError(DWORD error) noexcept {
    switch (error) {
        case ERROR_ACCESS_DENIED:
            return IoErrorAccessDenied;
        case ERROR_DISK_FULL:
            return IoErrorDiskFull;
        case ERROR_ALREADY_EXISTS:
            return IoErrorFileExists;
        case ERROR_INVALID_PARAMETER:
            return IoErrorInvalidArgument;
        case ERROR_SUCCESS:
            return IoErrorSuccess;
        case ERROR_FILE_NOT_FOUND:
        case ERROR_INVALID_DRIVE:
        case ERROR_PATH_NOT_FOUND:
        case ERROR_INVALID_NAME:
            return IoErrorNoSuchFileOrDirectory;
        default:
            LOG_WARN(Log::instance()->getLogger(), L"Unknown IO error - error=" << error);
            return IoErrorUnknown;
    }
}  // namespace

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

int IoHelper::_getAndSetRightsMethod = 0;

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

bool IoHelper::getFileStat(const SyncPath &path, FileStat *buf, bool &exists, IoError &ioError) noexcept {
    exists = true;
    ioError = IoErrorSuccess;

    if (!checkIfPathExists(path, exists, ioError)) {
        return false;
    }

    if (!exists) {
        return true;
    }

    // Get parent folder handle
    HANDLE hParent;
    bool retry = true;
    int counter = 50;
    while (retry) {
        retry = false;

        hParent = CreateFileW(path.parent_path().wstring().c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                              FILE_FLAG_BACKUP_SEMANTICS, NULL);

        if (hParent == INVALID_HANDLE_VALUE) {
            if (counter) {
                retry = true;
                Utility::msleep(10);
                LOGW_DEBUG(Log::instance()->getLogger(),
                           L"Retrying to get handle: " << Utility::formatSyncPath(path.parent_path()).c_str());
                counter--;
                continue;
            }

            LOG_WARN(logger(), L"Error in CreateFileW: " << Utility::formatSyncPath(path.parent_path()).c_str());
            ioError = dWordError2ioError(GetLastError());
            exists = false;

            return false;
        }
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
        exists = false;
        ioError = dWordError2ioError(dwError);

        return false;
    }

    // Get the Windows file id as an inode replacement.
    buf->inode = ((long long)pFileInfo->FileId.HighPart << 32) + (long long)pFileInfo->FileId.LowPart;
    buf->size = ((long long)pFileInfo->EndOfFile.HighPart << 32) + (long long)pFileInfo->EndOfFile.LowPart;

    DWORD rem;
    buf->modtime = FileTimeToUnixTime(pFileInfo->LastWriteTime, &rem);
    buf->creationTime = FileTimeToUnixTime(pFileInfo->CreationTime, &rem);

    bool isDirectory =
        !(pFileInfo->FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) && pFileInfo->FileAttributes & FILE_ATTRIBUTE_DIRECTORY;
    buf->type = isDirectory ? NodeTypeDirectory : NodeTypeFile;

    buf->isHidden = pFileInfo->FileAttributes & FILE_ATTRIBUTE_HIDDEN;
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
        return ioError == IoErrorNoSuchFileOrDirectory;
    }

    return true;
}

bool IoHelper::getXAttrValue(const SyncPath &path, DWORD attrCode, bool &value, IoError &ioError) noexcept {
    value = false;
    ioError = IoErrorSuccess;
    DWORD result = GetFileAttributesW(path.c_str());

    if (result == INVALID_FILE_ATTRIBUTES) {
        ioError = dWordError2ioError(GetLastError());

        return ioError == IoErrorNoSuchFileOrDirectory;
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

static bool setRightsWindowsApi(const SyncPath &path, DWORD permission, ACCESS_MODE accessMode, IoError &ioError,
                                log4cplus::Logger logger) noexcept {  // Return false if we should try the fallback method

    PACL pACL_old = nullptr;  // Current ACL
    PACL pACL_new = nullptr;  // New ACL
    PSECURITY_DESCRIPTOR pSecurityDescriptor = nullptr;
    EXPLICIT_ACCESS ExplicitAccess;
    ZeroMemory(&ExplicitAccess, sizeof(ExplicitAccess));

    ExplicitAccess.grfAccessPermissions = permission;
    ExplicitAccess.grfAccessMode = accessMode;
    ExplicitAccess.grfInheritance = NO_INHERITANCE;
    ExplicitAccess.Trustee.pMultipleTrustee = nullptr;
    ExplicitAccess.Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
    ExplicitAccess.Trustee.TrusteeForm = Utility::_trustee.TrusteeForm;
    ExplicitAccess.Trustee.TrusteeType = Utility::_trustee.TrusteeType;
    ExplicitAccess.Trustee.ptstrName = Utility::_trustee.ptstrName;

    std::wstring path_wstr = Path2WStr(path);
    size_t pathw_len = path_wstr.length();

    auto pathw_ptr = std::make_unique<WCHAR[]>(pathw_len + 1);
    path_wstr.copy(pathw_ptr.get(), pathw_len);

    LPCWSTR pathw_c = path_wstr.c_str();
    LPWSTR pathw = pathw_ptr.get();
    pathw[pathw_len] = L'\0';

    DWORD ValueReturned = GetNamedSecurityInfo(pathw_c, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, &pACL_old,
                                               nullptr, &pSecurityDescriptor);
    ioError = dWordError2ioError(ValueReturned);
    if (ValueReturned != ERROR_SUCCESS) {
        ioError = dWordError2ioError(ValueReturned);
        LOGW_WARN(logger, L"Error in GetNamedSecurityInfo: " << Utility::formatIoError(path, ioError).c_str()
                                                             << L" | DWORD error: " << ValueReturned);
        LocalFree(pSecurityDescriptor);
        LocalFree(pACL_new);
        // pACL_old is a pointer to the ACL in the security descriptor, so it should not be freed.
        if (ioError == IoErrorNoSuchFileOrDirectory || ioError == IoErrorAccessDenied) {
            return true;
        }
        return false;
    }

    ValueReturned = SetEntriesInAcl(1, &ExplicitAccess, pACL_old, &pACL_new);
    ioError = dWordError2ioError(ValueReturned);
    if (ValueReturned != ERROR_SUCCESS) {
        ioError = dWordError2ioError(ValueReturned);
        LOGW_WARN(logger, L"Error in SetEntriesInAcl: " << Utility::formatIoError(path, ioError).c_str() << L" | DWORD error: "
                                                        << ValueReturned);
        LocalFree(pSecurityDescriptor);
        LocalFree(pACL_new);
        // pACL_old is a pointer to the ACL in the security descriptor, so it should not be freed.
        if (ioError == IoErrorNoSuchFileOrDirectory || ioError == IoErrorAccessDenied) {
            return true;
        }
        return false;
    }

    if (!IsValidAcl(pACL_new)) {
        ioError = IoErrorUnknown;
        LOGW_WARN(logger, L"Invalid new ACL: " << Utility::formatSyncPath(path).c_str());

        LocalFree(pSecurityDescriptor);
        LocalFree(pACL_new);
        // pACL_old is a pointer to the ACL in the security descriptor, so it should not be freed.
        return false;
    }

    ValueReturned = SetNamedSecurityInfo(pathw, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, pACL_new, nullptr);
    ioError = dWordError2ioError(ValueReturned);
    if (ValueReturned != ERROR_SUCCESS) {
        ioError = dWordError2ioError(ValueReturned);
        LOGW_WARN(logger, L"Error in SetNamedSecurityInfo: " << Utility::formatIoError(path, ioError).c_str()
                                                             << L" | DWORD error: " << ValueReturned);
        LocalFree(pSecurityDescriptor);
        LocalFree(pACL_new);
        // pACL_old is a pointer to the ACL in the security descriptor, so it should not be freed.
        if (ioError == IoErrorNoSuchFileOrDirectory || ioError == IoErrorAccessDenied) {
            return true;
        }
        return false;
    }


    LocalFree(pSecurityDescriptor);
    LocalFree(pACL_new);
    // pACL_old is a pointer to the ACL in the security descriptor, so it should not be freed.

    return true;
}

static bool getRightsWindowsApi(const SyncPath &path, bool &read, bool &write, bool &exec, bool &exists,
                                log4cplus::Logger logger) noexcept {  // Return false if we should try the fallback method
    IoError ioError = IoErrorSuccess;
    WCHAR szFilePath[MAX_PATH_LENGTH_WIN_LONG];
    lstrcpyW(szFilePath, path.native().c_str());

    // Get security info
    PACL pfileACL = NULL;
    PSECURITY_DESCRIPTOR psecDesc = NULL;
    DWORD result =
        GetNamedSecurityInfo(szFilePath, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, &pfileACL, NULL, &psecDesc);
    LocalFree(psecDesc);
    ioError = dWordError2ioError(result);

    if (ioError != IoErrorSuccess) {
        exists = ioError != IoErrorNoSuchFileOrDirectory;

        if (ioError == IoErrorAccessDenied) {
            read = false;
            write = false;
            exec = false;
            exists = false;
            return true;  // Expected error if access is denied
        }

        if (exists) {
            LOGW_INFO(logger, L"GetNamedSecurityInfo failed: " << Utility::formatIoError(path, ioError).c_str()
                                                               << L" | DWORD error: " << result);
            return false;  // Unexpected error if the file exists but we can't get the security info (and it's not an access
                           // denied)
        }
        return true;  // Expected error if the file doesn't exist
    }

    // Get rights for trustee
    ACCESS_MASK rights = 0;
    result = GetEffectiveRightsFromAcl(pfileACL, &Utility::_trustee, &rights);
    ioError = dWordError2ioError(result);
    exists = ioError != IoErrorNoSuchFileOrDirectory;

    if (ioError == IoErrorAccessDenied) {
        read = false;
        write = false;
        exec = false;
        exists = false;
        LOGW_INFO(logger, L"GetEffectiveRightsFromAcl failed: " << Utility::formatIoError(path, ioError).c_str());
        return true;  // Expected error if access is denied
    }

    if (result == ERROR_INVALID_SID) {  // Access denied, try to force read control and get the rights
        setRightsWindowsApi(path, READ_CONTROL, ACCESS_MODE::GRANT_ACCESS, ioError, logger);
        GetNamedSecurityInfo(szFilePath, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, &pfileACL, NULL, &psecDesc);
        result = GetEffectiveRightsFromAcl(pfileACL, &Utility::_trustee, &rights);
        setRightsWindowsApi(path, READ_CONTROL, ACCESS_MODE::REVOKE_ACCESS, ioError, logger);
        ioError = dWordError2ioError(result);
    }

    if (ioError != IoErrorSuccess) {
        LOGW_INFO(logger,
                  L"Unexpected error: " << Utility::formatIoError(path, ioError).c_str() << L" | DWORD error: " << result);
        return false;  // Unexpected error if we can't get the rights
    }

    bool readCtrl = (rights & READ_CONTROL) == READ_CONTROL;  // Check if we have read control (needed to read the permissions)
    if (!readCtrl) {
        if (setRightsWindowsApi(path, READ_CONTROL, ACCESS_MODE::GRANT_ACCESS, ioError,
                                logger)) {  // Try to force read control
            GetNamedSecurityInfo(szFilePath, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, &pfileACL, NULL, &psecDesc);
            LocalFree(psecDesc);
            GetEffectiveRightsFromAcl(pfileACL, &Utility::_trustee, &rights);
            setRightsWindowsApi(path, READ_CONTROL, ACCESS_MODE::REVOKE_ACCESS, ioError,
                                logger);  // Revoke read control after reading the permissions
            readCtrl = (rights & READ_CONTROL) == READ_CONTROL;
        }
        if (!readCtrl) {
            ioError = IoErrorAccessDenied;
            LOGW_INFO(logger, L"Access denied: " << Utility::formatIoError(path, ioError).c_str());
            read = false;
            write = false;
            exec = false;
            exists = false;
            return true;  // Expected error if access is denied
        }
    }

    exists = true;
    read = (rights & FILE_GENERIC_READ) == FILE_GENERIC_READ;
    write = (rights & FILE_GENERIC_WRITE) == FILE_GENERIC_WRITE;
    exec = (rights & FILE_EXECUTE) == FILE_EXECUTE;
    return true;
}

bool IoHelper::getRights(const SyncPath &path, bool &read, bool &write, bool &exec, bool &exists) noexcept {
    // !!! When Full control to file/directory is denied to the user, the function will return as if the file/directory does not exist.

    read = false;
    write = false;
    exec = false;
    exists = false;
    // Preferred method
    if (_getAndSetRightsMethod == 0 && Utility::_trustee.ptstrName) {
        if (getRightsWindowsApi(path, read, write, exec, exists, logger())) {
            return true;
        }
        LOGW_WARN(logger(), L"Failed to get rights using Windows API, falling back to std::filesystem.");
        sentry_value_t event = sentry_value_new_event();
        sentry_value_t exc = sentry_value_new_exception(
            "Exception", "Failed to set/get rights using Windows API, falling back to std::filesystem.");
        sentry_value_set_stacktrace(exc, NULL, 0);
        sentry_event_add_exception(event, exc);
        sentry_capture_event(event);
        Utility::_trustee.ptstrName = nullptr;
        _getAndSetRightsMethod = 1;
    }

    // Fallback method.
    IoError ioError;
    ItemType itemType;
    const bool success = getItemType(path, itemType);
    ioError = itemType.ioError;

    if (!success) {
        LOGW_WARN(logger(), L"Failed to get item type: " << Utility::formatIoError(path, ioError).c_str());
        return false;
    }

    exists = ioError != IoErrorNoSuchFileOrDirectory;
    if (!exists) {
        return true;
    }
    const bool isSymlink = itemType.linkType == LinkTypeSymlink;

    std::error_code ec;
    std::filesystem::perms perms =
        isSymlink ? std::filesystem::symlink_status(path, ec).permissions() : std::filesystem::status(path, ec).permissions();
    ioError = stdError2ioError(ec);
    if (ioError != IoErrorSuccess) {
        exists = ioError != IoErrorNoSuchFileOrDirectory;
        if (!exists) {
            return true;
        }

        LOGW_WARN(logger(), L"Failed to get permissions: " << Utility::formatStdError(path, ec).c_str());
        return false;
    }

    exists = true;
    read = ((perms & std::filesystem::perms::owner_read) != std::filesystem::perms::none);
    write = ((perms & std::filesystem::perms::owner_write) != std::filesystem::perms::none);
    exec = ((perms & std::filesystem::perms::owner_exec) != std::filesystem::perms::none);
    return true;
}

bool IoHelper::setRights(const SyncPath &path, bool read, bool write, bool exec, IoError &ioError) noexcept {
    // Preferred method
    if (_getAndSetRightsMethod == 0 && Utility::_trustee.ptstrName) {
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
        bool res = false;
        res = setRightsWindowsApi(path, grantedPermission, ACCESS_MODE::SET_ACCESS, ioError, logger());
        if (res) {
            res &= setRightsWindowsApi(path, deniedPermission, ACCESS_MODE::DENY_ACCESS, ioError, logger());
        }
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
        Utility::_trustee.ptstrName = nullptr;
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
        return ioError == IoErrorNoSuchFileOrDirectory;
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
            return ioError == IoErrorNoSuchFileOrDirectory;
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
        return ioError == IoErrorNoSuchFileOrDirectory;
    }

    BYTE buf[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];
    REPARSE_DATA_BUFFER &reparseBuffer = (REPARSE_DATA_BUFFER &)buf;
    DWORD dwRet;
    if (!DeviceIoControl(hFile, FSCTL_GET_REPARSE_POINT, NULL, 0, &reparseBuffer, MAXIMUM_REPARSE_DATA_BUFFER_SIZE, &dwRet,
                         NULL)) {
        DWORD dwError = GetLastError();
        CloseHandle(hFile);

        ioError = dWordError2ioError(dwError);
        return ioError == IoErrorNoSuchFileOrDirectory;
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
    reparseDataBuffer->MountPointReparseBuffer.SubstituteNameLength = targetPathWLen * sizeof(WCHAR);
    reparseDataBuffer->MountPointReparseBuffer.PrintNameOffset = (targetPathWLen + 1) * sizeof(WCHAR);
    reparseDataBuffer->MountPointReparseBuffer.PrintNameLength = targetPathWLen * sizeof(WCHAR);
    reparseDataBuffer->ReparseDataLength = (targetPathWLen + 1) * 2 * sizeof(WCHAR) + REPARSE_MOUNTPOINT_HEADER_SIZE;

    DWORD dwError;
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
