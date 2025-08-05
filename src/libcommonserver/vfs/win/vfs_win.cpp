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

#include "vfs_win.h"
#include "libcommon/utility/utility.h"
#include "version.h"
#include "config.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/io/filestat.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/io/permissionsholder.h"

#include <unordered_map>
#include <shobjidl_core.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>

#include <log4cplus/loggingmacros.h>

namespace KDC {
VfsWin::VfsWin(const VfsSetupParams &vfsSetupParams, QObject *parent) :
    Vfs(vfsSetupParams, parent) {
    // Initialize LiteSync ext connector
    LOG_INFO(logger(), "Initialize LiteSyncExtConnector");
    TraceCbk debugCallback = std::bind(&VfsWin::debugCbk, this, std::placeholders::_1, std::placeholders::_2);

    Utility::setLogger(logger());
    IoHelper::setLogger(logger());

    if (vfsInit(debugCallback, QString(APPLICATION_NAME).toStdWString().c_str(), (DWORD) _getpid(),
                KDC::CommonUtility::escape(KDRIVE_VERSION_STRING).toStdWString().c_str(),
                QString(APPLICATION_TRASH_URL).toStdWString().c_str()) != S_OK) {
        LOG_WARN(logger(), "Error in vfsInit!");
        throw std::runtime_error("Error in vfsInit!");
        return;
    }

    starVfsWorkers();
}

void VfsWin::debugCbk(TraceLevel level, const wchar_t *msg) {
    switch (level) {
        case TraceLevel::Info:
            LOGW_INFO(logger(), msg)
            break;
        case TraceLevel::Debug:
            LOGW_DEBUG(logger(), msg)
            break;
        case TraceLevel::Warning:
            LOGW_WARN(logger(), msg)
            break;
        case TraceLevel::Error:
            LOGW_ERROR(logger(), msg)
            break;
    }
}

VirtualFileMode VfsWin::mode() const {
    return VirtualFileMode::Win;
}

ExitInfo VfsWin::startImpl(bool &, bool &, bool &) {
    LOG_DEBUG(logger(), "startImpl: syncDbId=" << _vfsSetupParams.syncDbId);

    wchar_t clsid[39] = L"";
    unsigned long clsidSize = sizeof(clsid);
    if (vfsStart(std::to_wstring(_vfsSetupParams.driveId).c_str(), std::to_wstring(_vfsSetupParams.userId).c_str(),
                 std::to_wstring(_vfsSetupParams.syncDbId).c_str(), _vfsSetupParams.localPath.filename().native().c_str(),
                 _vfsSetupParams.localPath.lexically_normal().native().c_str(), clsid, &clsidSize) != S_OK) {
        LOG_WARN(logger(), "Error in vfsStart: syncDbId=" << _vfsSetupParams.syncDbId);
        return {ExitCode::SystemError, ExitCause::UnableToCreateVfs};
    }

    _vfsSetupParams.namespaceCLSID = CommonUtility::ws2s(std::wstring(clsid));

    return ExitCode::Ok;
}

void VfsWin::stopImpl(bool unregister) {
    LOG_DEBUG(logger(), "stop: syncDbId=" << _vfsSetupParams.syncDbId);

    if (vfsStop(std::to_wstring(_vfsSetupParams.driveId).c_str(), std::to_wstring(_vfsSetupParams.syncDbId).c_str(),
                unregister) != S_OK) {
        LOG_WARN(logger(), "Error in vfsStop: syncDbId=" << _vfsSetupParams.syncDbId);
    }

    LOG_DEBUG(logger(), "stop done: syncDbId=" << _vfsSetupParams.syncDbId);
}

void VfsWin::dehydrate(const SyncPath &path) {
    LOGW_DEBUG(logger(), L"dehydrate: " << Utility::formatSyncPath(path));
    SyncPath relativePath = CommonUtility::relativePath(_vfsSetupParams.localPath, path);

    // Check file status
    SyncFileStatus status;
    _syncFileStatus(_vfsSetupParams.syncDbId, relativePath, status);
    if (status == SyncFileStatus::Unknown) {
        // The file is not synchronized, do nothing
        LOGW_DEBUG(logger(), L"Cannot dehydrate an unsynced file with " << Utility::formatSyncPath(path));
        return;
    }

    // Dehydrate file
    if (vfsDehydratePlaceHolder(path.native().c_str()) != S_OK) {
        LOGW_WARN(logger(), L"Error in vfsDehydratePlaceHolder: " << Utility::formatSyncPath(path));
    }

    _setSyncFileSyncing(_vfsSetupParams.syncDbId, relativePath, false);
}

void VfsWin::hydrate(const SyncPath &pathStd) {
    QString path = SyncName2QStr(pathStd.native());
    LOGW_DEBUG(logger(), L"hydrate: " << Utility::formatSyncPath(QStr2Path(path)));

    if (vfsHydratePlaceHolder(std::to_wstring(_vfsSetupParams.driveId).c_str(), std::to_wstring(_vfsSetupParams.syncDbId).c_str(),
                              QStr2Path(QDir::toNativeSeparators(path)).c_str()) != S_OK) {
        LOGW_WARN(logger(), L"Error in vfsHydratePlaceHolder: " << Utility::formatSyncPath(QStr2Path(path)));
    }

    QString relativePath = QStringView(path).mid(_vfsSetupParams.localPath.native().size() + 1).toUtf8();
    _setSyncFileSyncing(_vfsSetupParams.syncDbId, QStr2Path(relativePath), false);
}

void VfsWin::cancelHydrate(const SyncPath &pathStd) {
    LOGW_DEBUG(logger(), L"cancelHydrate: " << Utility::formatSyncPath(pathStd));
    const QString path = SyncName2QStr(pathStd.native());
    if (vfsCancelFetch(std::to_wstring(_vfsSetupParams.driveId).c_str(), std::to_wstring(_vfsSetupParams.syncDbId).c_str(),
                       QStr2Path(QDir::toNativeSeparators(path)).c_str()) != S_OK) {
        LOGW_WARN(logger(), L"Error in vfsCancelFetch: " << Utility::formatSyncPath(QStr2Path(path)));
        return;
    }

    QString relativePath = QStringView(path).mid(_vfsSetupParams.localPath.native().size() + 1).toUtf8();
    _setSyncFileSyncing(_vfsSetupParams.syncDbId, QStr2Path(relativePath), false);
}

void VfsWin::exclude(const SyncPath &pathStd) {
    const QString path = SyncName2QStr(pathStd.native());
    LOGW_DEBUG(logger(), L"exclude: " << Utility::formatSyncPath(QStr2Path(path)));

    DWORD dwAttrs = GetFileAttributesW(QStr2Path(QDir::toNativeSeparators(path)).c_str());
    if (dwAttrs == INVALID_FILE_ATTRIBUTES) {
        DWORD errorCode = GetLastError();
        LOGW_WARN(logger(),
                  L"Error in GetFileAttributesW: " << Utility::formatSyncPath(QStr2Path(path)) << L" code=" << errorCode);
        return;
    }

    if (vfsSetPinState(QStr2Path(QDir::toNativeSeparators(path)).c_str(), VFS_PIN_STATE_EXCLUDED) != S_OK) {
        LOGW_WARN(logger(), L"Error in vfsSetPinState: " << Utility::formatSyncPath(QStr2Path(path)));
        return;
    }
}

ExitInfo VfsWin::setPlaceholderStatus(const SyncPath &path, bool syncOngoing) {
    if (vfsSetPlaceHolderStatus(path.native().c_str(), syncOngoing) != S_OK) {
        LOGW_WARN(logger(), L"Error in vfsSetPlaceHolderStatus: " << Utility::formatSyncPath(path));
        return handleVfsError(path);
    }
    return ExitCode::Ok;
}

ExitInfo VfsWin::updateMetadata(const SyncPath &filePathStd, time_t creationTime, time_t modificationTime, int64_t size,
                                const NodeId &) {
    const QString filePath = SyncName2QStr(filePathStd.native());
    LOGW_DEBUG(logger(), L"updateMetadata: " << Utility::formatSyncPath(QStr2Path(filePath)) << L" creationTime=" << creationTime
                                             << L" modificationTime=" << modificationTime);

    SyncPath fullPath(_vfsSetupParams.localPath / QStr2Path(filePath));
    if (ExitInfo exitInfo = checkIfPathIsValid(fullPath, true); !exitInfo) {
        return exitInfo;
    }

    // Update placeholder
    PermissionsHolder permsHolder(fullPath);
    WIN32_FIND_DATA findData;
    findData.nFileSizeHigh = (DWORD) (size >> 32);
    findData.nFileSizeLow = (DWORD) (size & 0xFFFFFFFF);
    Utility::unixTimeToFiletime(modificationTime, &findData.ftLastWriteTime);
    findData.ftLastAccessTime = findData.ftLastWriteTime;
    Utility::unixTimeToFiletime(creationTime, &findData.ftCreationTime);
    findData.dwFileAttributes = GetFileAttributesW(QStr2Path(filePath).c_str());

    if (vfsUpdatePlaceHolder(QStr2Path(QDir::toNativeSeparators(filePath)).c_str(), &findData) != S_OK) {
        LOGW_WARN(logger(), L"Error in vfsUpdatePlaceHolder: " << Utility::formatSyncPath(fullPath));
        return handleVfsError(fullPath);
    }

    return ExitCode::Ok;
}

ExitInfo VfsWin::createPlaceholder(const SyncPath &relativeLocalPath, const SyncFileItem &item) {
    LOGW_DEBUG(logger(), L"createPlaceholder: " << Utility::formatSyncPath(relativeLocalPath));

    if (relativeLocalPath.empty()) {
        LOG_WARN(logger(), "VfsWin::createPlaceholder - relativeLocalPath cannot be empty.");
        return {ExitCode::SystemError, ExitCause::InvalidArgument};
    }

    if (!item.remoteNodeId().has_value()) {
        LOGW_WARN(logger(), L"VfsWin::createPlaceholder - Item has no remote ID: " << Utility::formatSyncPath(relativeLocalPath));
        return {ExitCode::SystemError, ExitCause::InvalidArgument};
    }

    SyncPath fullPath(_vfsSetupParams.localPath / relativeLocalPath);
    if (ExitInfo exitInfo = checkIfPathIsValid(fullPath, false); !exitInfo) {
        return exitInfo;
    }
    if (ExitInfo exitInfo = checkIfPathIsValid(fullPath.parent_path(), true); !exitInfo) {
        return exitInfo;
    }

    // Create placeholder
    PermissionsHolder permsHolder(fullPath);
    WIN32_FIND_DATA findData;
    findData.nFileSizeHigh = (DWORD) (item.size() >> 32);
    findData.nFileSizeLow = (DWORD) (item.size() & 0xFFFFFFFF);
    Utility::unixTimeToFiletime(item.creationTime(), &findData.ftCreationTime);
    Utility::unixTimeToFiletime(item.modTime(), &findData.ftLastWriteTime);
    findData.ftLastAccessTime = findData.ftLastWriteTime;
    findData.dwFileAttributes = (item.type() == NodeType::Directory ? FILE_ATTRIBUTE_DIRECTORY : 0);

    if (vfsCreatePlaceHolder(CommonUtility::s2ws(item.remoteNodeId().value()).c_str(),
                             relativeLocalPath.lexically_normal().native().c_str(),
                             _vfsSetupParams.localPath.lexically_normal().native().c_str(), &findData) != S_OK) {
        LOGW_WARN(logger(), L"Error in vfsCreatePlaceHolder: " << Utility::formatSyncPath(fullPath));
        return defaultVfsError(); // handleVfsError is not suitable here, the file dosen't exist but we don't want to return
                                  // NotFound as this make no sense in the context of a create
    }

    return ExitCode::Ok;
}

ExitInfo VfsWin::dehydratePlaceholder(const SyncPath &path) {
    LOGW_DEBUG(logger(), L"dehydratePlaceholder: " << Utility::formatSyncPath(path));
    SyncPath fullPath(_vfsSetupParams.localPath / path);

    if (ExitInfo exitInfo = checkIfPathIsValid(fullPath, true); !exitInfo) {
        return exitInfo;
    }

    // Check if the file is a placeholder
    bool isPlaceholder;
    if (vfsGetPlaceHolderStatus(fullPath.lexically_normal().native().c_str(), &isPlaceholder, nullptr, nullptr) != S_OK) {
        LOGW_WARN(logger(), L"Error in vfsGetPlaceHolderStatus: " << Utility::formatSyncPath(fullPath));
        return handleVfsError(fullPath);
    }

    if (!isPlaceholder) {
        // Not a placeholder
        LOGW_WARN(logger(), L"Not a placeholder: " << Utility::formatSyncPath(fullPath));
        return {ExitCode::SystemError, ExitCause::NotPlaceHolder};
    }

    LOGW_DEBUG(logger(), L"Dehydrate file: " << Utility::formatSyncPath(fullPath));
    PermissionsHolder permsHolder(fullPath);
    auto dehydrateFct = [=]() { dehydrate(fullPath.lexically_normal().native()); };
    std::thread dehydrateTask(dehydrateFct);
    dehydrateTask.detach();

    return ExitCode::Ok;
}

ExitInfo VfsWin::convertToPlaceholder(const SyncPath &pathStd, const SyncFileItem &item) {
    const QString path = SyncName2QStr(pathStd.native());
    LOGW_DEBUG(logger(), L"convertToPlaceholder: " << Utility::formatSyncPath(QStr2Path(path)));

    if (path.isEmpty()) {
        LOG_WARN(logger(), "Empty path!");
        return {ExitCode::SystemError, ExitCause::NotFound};
    }

    SyncPath fullPath(QStr2Path(path));
    PermissionsHolder permsHolder(fullPath);
    DWORD dwAttrs = GetFileAttributesW(fullPath.lexically_normal().native().c_str());
    if (dwAttrs == INVALID_FILE_ATTRIBUTES) {
        DWORD errorCode = GetLastError();
        LOGW_WARN(logger(), L"Error in GetFileAttributesW: " << Utility::formatSyncPath(fullPath) << L" code=" << errorCode);
        return handleVfsError(fullPath);
    }

    if (dwAttrs & FILE_ATTRIBUTE_DEVICE) {
        LOGW_DEBUG(logger(), L"Not a valid file or directory: " << Utility::formatSyncPath(fullPath));
        return handleVfsError(fullPath);
    }

    // Check if the file is already a placeholder
    bool isPlaceholder = false;
    if (vfsGetPlaceHolderStatus(fullPath.lexically_normal().native().c_str(), &isPlaceholder, nullptr, nullptr) != S_OK) {
        LOGW_WARN(logger(), L"Error in vfsGetPlaceHolderStatus: " << Utility::formatSyncPath(fullPath));
        return handleVfsError(fullPath);
    }

    if (!item.localNodeId().has_value()) {
        LOGW_WARN(logger(), L"Item has no local ID: " << Utility::formatSyncPath(fullPath));
        return {ExitCode::LogicError, ExitCause::InvalidArgument};
    }

    if (!isPlaceholder && (vfsConvertToPlaceHolder(CommonUtility::s2ws(item.localNodeId().value()).c_str(),
                                                   fullPath.lexically_normal().native().c_str()) != S_OK)) {
        LOGW_WARN(logger(), L"Error in vfsConvertToPlaceHolder: " << Utility::formatSyncPath(fullPath));
        return handleVfsError(fullPath);
    }
    return ExitCode::Ok;
}

void VfsWin::convertDirContentToPlaceholder(const QString &filePath, bool isHydratedIn) {
    const QFileInfoList infoList = QDir(filePath).entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    for (const auto &tmpInfo: qAsConst(infoList)) {
        QString tmpPath(tmpInfo.filePath());
        if (tmpPath.isEmpty()) {
            LOG_WARN(logger(), "Invalid parameters");
            continue;
        }

        SyncPath fullPath(QStr2Path(tmpPath));

        if (ExitInfo exitInfo = checkIfPathIsValid(fullPath, true); !exitInfo) {
            if (exitInfo == ExitInfo(ExitCode::SystemError, ExitCause::NotFound)) {
                // File creation and rename
                LOGW_DEBUG(logger(), L"File doesn't exist: " << Utility::formatSyncPath(fullPath));
                continue;
            }
            LOGW_WARN(logger(), L"Error in checkIfPathIsValid: " << Utility::formatSyncPath(fullPath) << L" " << exitInfo);
            return;
        }

        // Check if the file is already a placeholder
        bool isPlaceholder = false;
        if (vfsGetPlaceHolderStatus(fullPath.lexically_normal().native().c_str(), &isPlaceholder, nullptr, nullptr) != S_OK) {
            LOGW_WARN(logger(), L"Error in vfsGetPlaceHolderStatus: " << Utility::formatSyncPath(fullPath));
            break;
        }

        if (!isPlaceholder) {
            FileStat fileStat;
            IoError ioError = IoError::Success;
            if (!IoHelper::getFileStat(fullPath, &fileStat, ioError)) {
                LOGW_WARN(logger(), L"Error in IoHelper::getFileStat: " << Utility::formatIoError(fullPath, ioError));
                break;
            }

            if (ioError == IoError::NoSuchFileOrDirectory) {
                LOGW_DEBUG(logger(), L"Directory entry does not exist anymore: " << Utility::formatSyncPath(fullPath));
                continue;
            } else if (ioError == IoError::AccessDenied) {
                LOGW_WARN(logger(), L"Item: " << Utility::formatSyncPath(fullPath) << L" rejected because access is denied");
                continue;
            }

            // Convert to placeholder
            {
                PermissionsHolder permsHolder(fullPath);
                if (vfsConvertToPlaceHolder(std::to_wstring(fileStat.inode).c_str(),
                                            fullPath.lexically_normal().native().c_str()) != S_OK) {
                    LOGW_WARN(logger(), L"Error in vfsConvertToPlaceHolder: " << Utility::formatSyncPath(fullPath));
                    break;
                }
            }

            if (tmpInfo.isDir()) {
                convertDirContentToPlaceholder(tmpPath, isHydratedIn);
            }
        }
    }
}

void VfsWin::clearFileAttributes(const SyncPath &fullPath) {
    PermissionsHolder permsHolder(fullPath);
    if (vfsRevertPlaceHolder(fullPath.lexically_normal().native().c_str()) != S_OK) {
        LOGW_WARN(logger(), L"Error in vfsRevertPlaceHolder: " << Utility::formatSyncPath(fullPath));
    }
}

ExitInfo VfsWin::updateFetchStatus(const SyncPath &tmpPathStd, const SyncPath &pathStd, int64_t received, bool &canceled,
                                   bool &finished) {
    Q_UNUSED(finished)
    const QString tmpPath = SyncName2QStr(tmpPathStd.native());
    const QString path = SyncName2QStr(pathStd.native());

    LOGW_DEBUG(logger(), L"updateFetchStatus: " << Utility::formatSyncPath(QStr2Path(path)));

    if (tmpPath.isEmpty()) {
        LOG_WARN(logger(), "Invalid parameters");
        return {ExitCode::SystemError, ExitCause::InvalidArgument};
    }

    SyncPath fullTmpPath(QStr2Path(tmpPath));
    SyncPath fullPath(QStr2Path(path));

    if (ExitInfo exitInfo = checkIfPathIsValid(fullPath, true); !exitInfo) {
        if (exitInfo == ExitInfo(ExitCode::SystemError, ExitCause::NotFound)) {
            return ExitCode::Ok;
        }
    }

    // Check if the file is a placeholder
    bool isPlaceholder = false;
    if (vfsGetPlaceHolderStatus(fullPath.lexically_normal().native().c_str(), &isPlaceholder, nullptr, nullptr) != S_OK) {
        LOGW_WARN(logger(), L"Error in vfsGetPlaceHolderStatus: " << Utility::formatSyncPath(fullPath));
        return handleVfsError(fullPath);
    }

    PermissionsHolder permsHolder(fullPath);
    if (vfsUpdateFetchStatus(std::to_wstring(_vfsSetupParams.driveId).c_str(), std::to_wstring(_vfsSetupParams.syncDbId).c_str(),
                             fullPath.lexically_normal().native().c_str(), fullTmpPath.lexically_normal().native().c_str(),
                             received, &canceled, &finished) != S_OK) {
        LOGW_WARN(logger(), L"Error in vfsUpdateFetchStatus: " << Utility::formatSyncPath(fullPath));
        return handleVfsError(fullPath);
    }
    return ExitCode::Ok;
}

ExitInfo VfsWin::forceStatus(const SyncPath &absolutePathStd, const VfsStatus &vfsStatus) {
    QString absolutePath = SyncName2QStr(absolutePathStd.native());
    if (ExitInfo exitInfo = checkIfPathIsValid(absolutePathStd, true); !exitInfo) {
        return exitInfo;
    }

    DWORD dwAttrs = GetFileAttributesW(absolutePathStd.native().c_str());
    if (dwAttrs == INVALID_FILE_ATTRIBUTES) {
        DWORD errorCode = GetLastError();
        LOGW_WARN(logger(),
                  L"Error in GetFileAttributesW: " << Utility::formatSyncPath(absolutePathStd) << L" code=" << errorCode);
        return handleVfsError(absolutePathStd);
    }

    if (dwAttrs & FILE_ATTRIBUTE_DEVICE) {
        LOGW_WARN(logger(), L"Not a valid file or directory: " << Utility::formatSyncPath(absolutePathStd));
        return handleVfsError(absolutePathStd);
    }

    bool isPlaceholder = false;
    if (vfsGetPlaceHolderStatus(absolutePathStd.native().c_str(), &isPlaceholder, nullptr, nullptr) != S_OK) {
        LOGW_WARN(logger(), L"Error in vfsGetPlaceHolderStatus: " << Utility::formatSyncPath(absolutePathStd));
        return handleVfsError(absolutePathStd);
    }

    // Some editors (notepad++) seems to remove the file attributes, therefore we need to verify that the file is still a
    // placeholder
    if (!isPlaceholder) {
        FileStat filestat;
        auto ioError = IoError::Success;
        if (!IoHelper::getFileStat(absolutePathStd, &filestat, ioError)) {
            LOGW_WARN(logger(), L"Error in IoHelper::getFileStat: " << Utility::formatIoError(absolutePathStd, ioError));
            return ExitCode::SystemError;
        }
        if (ioError == IoError::NoSuchFileOrDirectory) {
            LOGW_DEBUG(logger(), L"Item does not exist anymore: " << Utility::formatSyncPath(absolutePathStd));
            return {ExitCode::SystemError, ExitCause::NotFound};
        } else if (ioError == IoError::AccessDenied) {
            LOGW_WARN(logger(), L"Access is denied for item: " << Utility::formatSyncPath(absolutePathStd));
            return {ExitCode::SystemError, ExitCause::FileAccessError};
        }

        NodeId localNodeId = std::to_string(filestat.inode);

        // Convert to placeholder
        PermissionsHolder permsHolder(absolutePathStd);
        if (vfsConvertToPlaceHolder(CommonUtility::s2ws(localNodeId).c_str(), absolutePathStd.native().c_str()) != S_OK) {
            LOGW_WARN(logger(), L"Error in vfsConvertToPlaceHolder: " << Utility::formatSyncPath(absolutePathStd));
            return handleVfsError(absolutePathStd);
        }
    }

    // Set status
    LOGW_DEBUG(logger(), L"Setting syncing status to: " << vfsStatus.isSyncing << L" for file: "
                                                        << Utility::formatSyncPath(absolutePathStd));
    PermissionsHolder permsHolder(absolutePathStd);
    if (ExitInfo exitInfo = setPlaceholderStatus(absolutePathStd.native(), vfsStatus.isSyncing); !exitInfo) {
        LOGW_WARN(logger(), L"Error in setPlaceholderStatus: " << Utility::formatSyncPath(absolutePathStd) << L" " << exitInfo);
        return exitInfo;
    }

    return ExitCode::Ok;
}

ExitInfo VfsWin::isDehydratedPlaceholder(const SyncPath &initFilePathStd, bool &isDehydrated) {
    QString initFilePath = SyncName2QStr(initFilePathStd.native());
    SyncPath filePath(_vfsSetupParams.localPath / initFilePathStd.native());

    if (vfsGetPlaceHolderStatus(filePath.lexically_normal().native().c_str(), nullptr, &isDehydrated, nullptr) != S_OK) {
        LOGW_WARN(logger(), L"Error in vfsGetPlaceHolderStatus: " << Utility::formatSyncPath(filePath));
        return handleVfsError(filePath);
    }

    return ExitCode::Ok;
}

ExitInfo VfsWin::setPinState(const SyncPath &relativePathStd, PinState state) {
    // We currently don't have any features that require pinning/unpinning files manually on windows.
    return ExitCode::Ok;

    QString relativePath = SyncName2QStr(relativePathStd.native());
    SyncPath fullPath(_vfsSetupParams.localPath / QStr2Path(relativePath));
    DWORD dwAttrs = GetFileAttributesW(fullPath.lexically_normal().native().c_str());
    if (dwAttrs == INVALID_FILE_ATTRIBUTES) {
        DWORD errorCode = GetLastError();
        LOGW_WARN(logger(), L"Error in GetFileAttributesW: " << Utility::formatSyncPath(fullPath) << L" code=" << errorCode);
        return handleVfsError(fullPath);
    }

    VfsPinState vfsState;
    switch (state) {
        case PinState::Inherited:
            vfsState = VFS_PIN_STATE_INHERIT;
            break;
        case PinState::AlwaysLocal:
            vfsState = VFS_PIN_STATE_PINNED;
            break;
        case PinState::OnlineOnly:
            vfsState = VFS_PIN_STATE_UNPINNED;
            break;
        case PinState::Unspecified:
            vfsState = VFS_PIN_STATE_UNSPECIFIED;
            break;
        default:
            assert(false && "Invalid pin state");
            LOGW_WARN(logger(), L"Invalid pinState: " << state << L"in setPinState for: " << Utility::formatSyncPath(fullPath));
            vfsState = VFS_PIN_STATE_UNSPECIFIED;
            break;
    }

    PermissionsHolder permsHolder(fullPath);
    if (vfsSetPinState(fullPath.lexically_normal().native().c_str(), vfsState) != S_OK) {
        LOGW_WARN(logger(), L"Error in vfsSetPinState: " << Utility::formatSyncPath(fullPath));
        return handleVfsError(fullPath);
    }

    return ExitCode::Ok;
}

PinState VfsWin::pinState(const SyncPath &relativePathStd) {
    //  Read pin state from file attributes
    SyncPath fullPath(_vfsSetupParams.localPath / relativePathStd.native());
    VfsPinState vfsPinState = VFS_PIN_STATE_UNKNOWN;
    vfsGetPinState(fullPath.lexically_normal().native().c_str(), &vfsPinState);
    PinState state = PinState::Unknown;
    switch (vfsPinState) {
        case VFS_PIN_STATE_PINNED:
            state = PinState::AlwaysLocal;
            break;
        case VFS_PIN_STATE_UNPINNED:
            state = PinState::OnlineOnly;
            break;
        case VFS_PIN_STATE_UNSPECIFIED:
            state = PinState::Unspecified;
            break;
        case VFS_PIN_STATE_EXCLUDED:
            state = PinState::Unspecified;
            break;
        default:
            assert(false && "Invalid pin state");
            LOGW_WARN(logger(),
                      L"Invalid pinState: " << vfsPinState << L" in getPinState for: " << Utility::formatSyncPath(fullPath));
            state = PinState::Unknown;
            break;
    }
    return state;
}

ExitInfo VfsWin::status(const SyncPath &filePath, VfsStatus &vfsStatus) {
    // Check if the file is a placeholder
    PermissionsHolder permsHolder(filePath);
    bool isDehydrated = false;
    if (vfsGetPlaceHolderStatus(filePath.lexically_normal().native().c_str(), &vfsStatus.isPlaceholder, &isDehydrated, nullptr) !=
        S_OK) {
        LOGW_WARN(logger(), L"Error in vfsGetPlaceHolderStatus: " << Utility::formatSyncPath(filePath));
        return handleVfsError(filePath);
    }

    vfsStatus.isHydrated = !isDehydrated;
    vfsStatus.isSyncing = false;

    return ExitCode::Ok;
}

bool VfsWin::fileStatusChanged(const SyncPath &pathStd, SyncFileStatus status) {
    LOGW_DEBUG(logger(), L"fileStatusChanged: " << Utility::formatSyncPath(pathStd) << L" status = " << status);
    const QString path = SyncName2QStr(pathStd.native());
    const SyncPath fullPath(pathStd.native());
    if (ExitInfo exitInfo = checkIfPathIsValid(fullPath, true); !exitInfo) {
        if (exitInfo == ExitInfo(ExitCode::SystemError, ExitCause::NotFound)) {
            return true;
        }
        return false;
    }
    const SyncPath fileRelativePath = CommonUtility::relativePath(_vfsSetupParams.localPath, fullPath);

    switch (status) {
        case SyncFileStatus::Conflict:
        case SyncFileStatus::Ignored:
            exclude(fullPath);
            break;
        case SyncFileStatus::Success: {
            bool isDirectory = false;

            if (IoError ioError = IoError::Success; !IoHelper::checkIfIsDirectory(fullPath, isDirectory, ioError)) {
                LOGW_WARN(logger(), L"Failed to check if path is a directory: " << Utility::formatIoError(fullPath, ioError));
                return false;
            }

            if (!isDirectory) {
                // File
                bool isDehydrated = false;
                if (ExitInfo exitInfo = isDehydratedPlaceholder(fileRelativePath, isDehydrated); !exitInfo) {
                    LOGW_WARN(logger(),
                              L"Error in isDehydratedPlaceholder: " << Utility::formatSyncPath(fullPath) << L" - " << exitInfo);
                    return false;
                }
                VfsStatus vfsStatus = {.isHydrated = !isDehydrated, .progress = 100};
                forceStatus(fullPath, vfsStatus);
            }
        } break;
        case SyncFileStatus::Syncing: {
            bool isDirectory = false;

            if (IoError ioError = IoError::Success; !IoHelper::checkIfIsDirectory(fullPath, isDirectory, ioError)) {
                LOGW_WARN(logger(), L"Failed to check if path is a directory: " << Utility::formatIoError(fullPath, ioError));
                return false;
            }
            if (isDirectory) break;
            // File
            auto localPinState = pinState(fileRelativePath);

            if (localPinState != PinState::OnlineOnly && localPinState != PinState::AlwaysLocal) break;

            bool isDehydrated = false;
            if (ExitInfo exitInfo = isDehydratedPlaceholder(fileRelativePath, isDehydrated); !exitInfo) {
                LOGW_WARN(logger(),
                          L"Error in isDehydratedPlaceholder: " << Utility::formatSyncPath(fullPath) << L" - " << exitInfo);
                return false;
            }

            bool syncing = false;
            _syncFileSyncing(_vfsSetupParams.syncDbId, fileRelativePath, syncing);

            if (localPinState == PinState::OnlineOnly && !isDehydrated) {
                // Add file path to dehydration queue
                _workerInfo[workerDehydration]._mutex.lock();
                _workerInfo[workerDehydration]._queue.push_front(fullPath);
                _workerInfo[workerDehydration]._mutex.unlock();
                _workerInfo[workerDehydration]._queueWC.wakeOne();
            } else if (localPinState == PinState::AlwaysLocal && isDehydrated && !syncing) {
                // Set hydrating indicator (avoid double hydration)
                _setSyncFileSyncing(_vfsSetupParams.syncDbId, fileRelativePath, true);

                // Add file path to hydration queue
                _workerInfo[workerHydration]._mutex.lock();
                _workerInfo[workerHydration]._queue.push_front(fullPath);
                _workerInfo[workerHydration]._mutex.unlock();
                _workerInfo[workerHydration]._queueWC.wakeOne();
            }

        } break;
        default:
            // Nothing to do
            break;
    }
    return true;
}
} // namespace KDC
