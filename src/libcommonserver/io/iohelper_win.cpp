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
#include <Accctrl.h>
#define SECURITY_WIN32
#include <security.h>
#include <ntstatus.h>

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
            return IoErrorNoSuchFileOrDirectory;
        default:
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

int IoHelper::_getRightsMethod = 0;

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

bool IoHelper::getRights(const SyncPath &path, bool &read, bool &write, bool &exec, bool &exists) noexcept {
    read = false;
    write = false;
    exec = false;
    exists = false;

    IoError ioError = IoErrorSuccess;
    for (;;) {
        switch (_getRightsMethod) {
            case 0: {
                if (Utility::_trustee.ptstrName) {
                    WCHAR szFilePath[MAX_PATH_LENGTH_WIN_LONG];
                    lstrcpyW(szFilePath, path.native().c_str());

                    // Get security info
                    PACL pfileACL = NULL;
                    PSECURITY_DESCRIPTOR psecDesc = NULL;
                    DWORD result = GetNamedSecurityInfoW(szFilePath, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL,
                                                         &pfileACL, NULL, &psecDesc);
                    ioError = dWordError2ioError(result);
                    if (ioError != IoErrorSuccess) {
                        exists = ioError != IoErrorNoSuchFileOrDirectory;
                        if (!exists) {
                            return true;
                        }

                        if (ioError == IoErrorAccessDenied) {
                            read = false;
                            write = false;
                            exec = false;
                            LOGW_INFO(logger(), L"Access denied : " << szFilePath);
                            return true;
                        }

                        LOGW_WARN(logger(), L"Error in GetNamedSecurityInfoW: " << szFilePath << L" result=" << result);
                    } else {
                        // Get rights for trustee
                        ACCESS_MASK rights = 0;
                        result = GetEffectiveRightsFromAcl(pfileACL, &Utility::_trustee, &rights);
                        ioError = dWordError2ioError(result);
                        LocalFree(psecDesc);
                        if (ioError != IoErrorSuccess) {
                            exists = ioError != IoErrorNoSuchFileOrDirectory;
                            if (!exists) {
                                return true;
                            }

                            if (ioError == IoErrorAccessDenied) {
                                read = false;
                                write = false;
                                exec = false;
                                LOGW_INFO(logger(), L"Access denied: " << szFilePath);
                                return true;
                            }

                            LOGW_WARN(logger(), L"Error in GetEffectiveRightsFromAcl: " << szFilePath << L" result=" << result);
                        } else {
                            exists = true;
                            read = (rights & FILE_GENERIC_READ) == FILE_GENERIC_READ;
                            write = (rights & FILE_GENERIC_WRITE) == FILE_GENERIC_WRITE;
                            exec = (rights & FILE_GENERIC_EXECUTE) == FILE_GENERIC_EXECUTE;
                            return true;
                        }
                    }
                }

                // Try next method
                IoHelper::_getRightsMethod++;
                Utility::_trustee = {0};
                LocalFree(Utility::_psid);
                Utility::_psid = NULL;
                break;
            }
            case 1: {
                // Fallback method
                // !!! When Deny Full control to file/directory => returns exists == false !!!

                ItemType itemType;
                const bool success = getItemType(path, itemType);
                ioError = itemType.ioError;
                if (!success) {
                    LOGW_WARN(logger(),
                              L"Failed to check if the item is a symlink: " << Utility::formatIoError(path, ioError).c_str());
                    return false;
                }

                exists = ioError != IoErrorNoSuchFileOrDirectory;
                if (!exists) {
                    return true;
                }
                const bool isSymlink = itemType.linkType == LinkTypeSymlink;

                std::error_code ec;
                std::filesystem::perms perms = isSymlink ? std::filesystem::symlink_status(path, ec).permissions()
                                                         : std::filesystem::status(path, ec).permissions();
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
                break;
            }
        }
    }

    return true;
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
