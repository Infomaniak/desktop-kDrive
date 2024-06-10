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

#include "folderwatcher_win.h"
#include "localfilesystemobserverworker.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"
#include "requests/parameterscache.h"

#include <type_traits>

namespace KDC {

#define NOTIFY_BUFFER_SIZE 64 * 1024

FolderWatcher_win::FolderWatcher_win(LocalFileSystemObserverWorker *parent, const SyncPath &path) : FolderWatcher(parent, path) {}

FolderWatcher_win::~FolderWatcher_win() {}

bool FolderWatcher_win::ready() const {
    return _ready;
}

void FolderWatcher_win::changesLost() {
    // Current snapshot needs to be invalidated
    _parent->invalidateSnapshot();
}

void FolderWatcher_win::changeDetected(const SyncPath &path, OperationType opType) {
    std::list<std::pair<SyncPath, OperationType>> list;
    list.push_back({path, opType});
    _parent->changesDetected(list);
}

void FolderWatcher_win::startWatching() {
    LOGW_DEBUG(_logger, L"Start watching folder: " << _folder.wstring().c_str());
    LOG_DEBUG(_logger, "File system format: " << Utility::fileSystemName(_folder).c_str());

    _resultEventHandle = CreateEvent(NULL, true, false, NULL);
    _stopEventHandle = CreateEvent(NULL, true, false, NULL);

    while (!_stop) {
        watchChanges();

        if (!_stop) {
            // Other errors shouldn't actually happen,
            // so sleep a bit to avoid running into the same error case in a
            // tight loop.
            Utility::msleep(100);
        }
    }

    LOGW_DEBUG(_logger, L"Folder watching stopped: " << _folder.wstring().c_str());
}

void FolderWatcher_win::stopWatching() {
    LOGW_DEBUG(_logger, L"Stop watching folder: " << Path2WStr(_folder).c_str());

    SetEvent(_stopEventHandle);
    closeHandle();
}

void FolderWatcher_win::watchChanges() {
    LOG_DEBUG(_logger, "Start watching changes");

    _directoryHandle =
        CreateFileW(_folder.native().c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE, NULL,
                    OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);

    if (_directoryHandle == INVALID_HANDLE_VALUE) {
        DWORD errorCode = GetLastError();
        LOGW_WARN(_logger, L"Failed to create handle for " << _folder.wstring().c_str() << L" - error:" << errorCode);
        _directoryHandle = nullptr;
        return;
    }

    std::aligned_storage_t<NOTIFY_BUFFER_SIZE, sizeof(DWORD)> fileNotifyBuffer;

    OVERLAPPED overlapped;
    overlapped.hEvent = _resultEventHandle;

    while (!_stop) {
        ResetEvent(_resultEventHandle);

        DWORD dwBytesReturned = 0;
        if (!ReadDirectoryChangesW(_directoryHandle, &fileNotifyBuffer, static_cast<DWORD>(NOTIFY_BUFFER_SIZE), true,
                                   FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_ATTRIBUTES |
                                       FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION |
                                       FILE_NOTIFY_CHANGE_SECURITY,
                                   &dwBytesReturned, &overlapped, NULL)) {
            DWORD errorCode = GetLastError();
            if (errorCode == ERROR_NOTIFY_ENUM_DIR) {
                LOG_DEBUG(_logger, "The buffer for changes overflowed! Fallback to static sync");
                changesLost();
            } else {
                LOG_WARN(_logger, "ReadDirectoryChangesW error " << errorCode);
            }
            break;
        }

        _ready = true;

        HANDLE handles[] = {_resultEventHandle, _stopEventHandle};
        DWORD result = WaitForMultipleObjects(2, handles, false  // awake once one of them arrives
                                              ,
                                              INFINITE);

        if (result == 1) {
            LOG_DEBUG(_logger, "Received stop event, aborting folder watcher thread");
            break;
        }

        if (result != 0) {
            LOG_WARN(_logger, "WaitForMultipleObjects failed " << result << GetLastError());
            break;
        }

        bool ok = GetOverlappedResult(_directoryHandle, &overlapped, &dwBytesReturned, false);
        if (!ok) {
            DWORD errorCode = GetLastError();
            if (errorCode == ERROR_NOTIFY_ENUM_DIR) {
                LOG_DEBUG(_logger, "The buffer for changes overflowed! Fallback to static sync");
                changesLost();
            } else {
                LOG_WARN(_logger, "GetOverlappedResult error " << errorCode);
            }
            break;
        }

        const FILE_NOTIFY_INFORMATION *notifInfo = reinterpret_cast<const FILE_NOTIFY_INFORMATION *>(&fileNotifyBuffer);
        while (!_stop) {
            std::wstring wPathName(notifInfo->FileName,
                                   notifInfo->FileName + (notifInfo->FileNameLength / sizeof(notifInfo->FileName)));
            SyncPath filepath = (_folder / wPathName).lexically_normal();

            bool skip = false;
            OperationType opType = operationFromAction(notifInfo->Action);

            if (notifInfo->Action == FILE_ACTION_MODIFIED) {
                bool isDirectory = false;
                IoError ioError = IoErrorSuccess;
                const bool isDirectorySuccess = IoHelper::checkIfIsDirectory(filepath, isDirectory, ioError);
                if (!isDirectorySuccess) {
                    LOGW_WARN(_logger,
                              L"Error in IoHelper::checkIfIsDirectory: " << Utility::formatIoError(filepath, ioError).c_str());
                }
                if (!isDirectory) {
                    if (ioError == IoErrorNoSuchFileOrDirectory) {
                        LOGW_DEBUG(_logger, L"Skip operation " << Utility::s2ws(Utility::opType2Str(opType)).c_str()
                                                              << L" detected on item " << Path2WStr(filepath).c_str()
                                                              << L" (item doesn't exist)");
                        skip = true;
                    }
                } else {
                    if (ParametersCache::isExtendedLogEnabled()) {
                        LOGW_DEBUG(_logger, L"Skip operation " << Utility::s2ws(Utility::opType2Str(opType)).c_str()
                                                              << L" detected on item " << Path2WStr(filepath).c_str()
                                                              << L" (directory)");
                    }
                    skip = true;
                }
            }

            // Unless the file was removed or renamed, get its full long name
            SyncPath longfilepath = filepath;
            if (!skip && notifInfo->Action != FILE_ACTION_REMOVED && notifInfo->Action != FILE_ACTION_RENAMED_OLD_NAME) {
                bool notFound = false;
                if (!KDC::Utility::longPath(filepath, longfilepath, notFound)) {
                    if (notFound) {
                        // Item doesn't exist anymore
                        LOGW_DEBUG(_logger, L"Skip operation " << Utility::s2ws(Utility::opType2Str(opType)).c_str()
                                                              << L" detected on item " << Path2WStr(longfilepath).c_str()
                                                              << L" (item doesn't exist)");
                        skip = true;
                    } else {
                        // Keep original name
                        LOGW_WARN(KDC::Log::instance()->getLogger(),
                                  L"Error in Utility::longpath for path=" << Path2WStr(filepath).c_str());
                    }
                }
            }

            if (!skip) {
                if (ParametersCache::isExtendedLogEnabled()) {
                    LOGW_DEBUG(_logger, L"Operation " << Utility::s2ws(Utility::opType2Str(opType)).c_str()
                                                     << L" detected on item " << Path2WStr(longfilepath).c_str());
                }

                changeDetected(longfilepath, opType);
            }

            if (notifInfo->NextEntryOffset == 0) {
                break;
            }

            notifInfo = reinterpret_cast<const FILE_NOTIFY_INFORMATION *>(reinterpret_cast<const char *>(notifInfo) +
                                                                          notifInfo->NextEntryOffset);
        }
    }

    CancelIo(_directoryHandle);
    closeHandle();

    LOG_DEBUG(_logger, "Watching changes stoped");
}

void FolderWatcher_win::closeHandle() {
    if (_directoryHandle) {
        try {
            CloseHandle(_directoryHandle);
        } catch (...) {
            LOG_WARN(_logger, "Failed to close directory handle");
        }

        _directoryHandle = nullptr;
    }
}

OperationType FolderWatcher_win::operationFromAction(DWORD action) {
    switch (action) {
        case FILE_ACTION_RENAMED_OLD_NAME:
            return OperationTypeMove;
            break;
        case FILE_ACTION_RENAMED_NEW_NAME:
            return OperationTypeMove;
            break;
        case FILE_ACTION_ADDED:
            return OperationTypeCreate;
            break;
        case FILE_ACTION_REMOVED:
            return OperationTypeDelete;
            break;
        case FILE_ACTION_MODIFIED:
            return OperationTypeEdit;
            break;
    }

    return OperationTypeNone;
}

}  // namespace KDC
