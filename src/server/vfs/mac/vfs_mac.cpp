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

#include "vfs_mac.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/utility/utility.h"

#include <QDir>
#include <QFile>

#include <iostream>
#include <sys/stat.h>

#include <log4cplus/loggingmacros.h>

namespace KDC {

VfsMac::VfsMac(const VfsSetupParams &vfsSetupParams, QObject *parent) :
    Vfs(vfsSetupParams, parent) {
    // Initialize LiteSync ext connector
    LOG_INFO(logger(), "Initialize LiteSyncExtConnector");

    Utility::setLogger(logger());
    IoHelper::setLogger(logger());

    _connector = LiteSyncCommClient::instance(logger(), vfsSetupParams.executeCommand);
    if (!_connector) {
        LOG_WARN(logger(), "Error in LiteSyncExtConnector::instance");
        throw std::runtime_error("Unable to initialize LiteSyncExtConnector.");
    }

    starVfsWorkers();
}

VirtualFileMode VfsMac::mode() const {
    return VirtualFileMode::Mac;
}

ExitInfo VfsMac::startImpl(bool &installationDone, bool &activationDone, bool &connectionDone) {
    LOG_DEBUG(logger(), "startImpl - syncDbId=" << _vfsSetupParams.syncDbId);

    if (!_connector) {
        LOG_WARN(logger(), "LiteSyncExtConnector not initialized!");
        return ExitCode::LogicError;
    }

    if (!installationDone) {
        activationDone = false;
        connectionDone = false;
        installationDone = _connector->install(activationDone);
        if (!installationDone) {
            LOG_WARN(logger(), "Error in LiteSyncExtConnector::install!");
            return {ExitCode::SystemError, ExitCause::UnableToCreateVfs};
        }
    }

    if (!activationDone) {
        LOG_INFO(logger(), "LiteSync extension activation pending");
        connectionDone = false;
        return {ExitCode::SystemError, ExitCause::UnableToCreateVfs};
    }

    if (!connectionDone) {
        connectionDone = _connector->connect();
        if (!connectionDone) {
            LOG_WARN(logger(), "Error in LiteSyncExtConnector::connect!");
            return {ExitCode::SystemError, ExitCause::UnableToCreateVfs};
        }
    }

    // Set app exclude list
    if (!setAppExcludeList()) {
        LOG_WARN(logger(), "Error in setAppExcludeList!");
    }

    bool isPlaceholder = false;
    bool isSyncing = false;
    if (!_connector->vfsStart(_vfsSetupParams.syncDbId, _vfsSetupParams.localPath, isPlaceholder, isSyncing)) {
        LOG_WARN(logger(), "Error in vfsStart!");
        resetLiteSyncConnector();
        return {ExitCode::SystemError, ExitCause::UnableToCreateVfs};
    }

    if (std::list<SyncPath> filesToFix;
        isPlaceholder && isSyncing &&
        _connector->checkFilesAttributes(_vfsSetupParams.localPath, _vfsSetupParams.localPath,
                                         filesToFix)) { // Verify that all files/folders are in the correct state
        // Get the directories to fix
        std::unordered_set<SyncPath> dirsToFix;
        for (const auto &filePath: filesToFix) {
            bool isDirectory = false;
            auto ioError = IoError::Success;
            if (const bool isDirSuccess = IoHelper::checkIfIsDirectory(filePath, isDirectory, ioError); !isDirSuccess) {
                LOGW_WARN(logger(), L"Error in IoHelper::checkIfIsDirectory: " << Utility::formatIoError(filePath, ioError));
                continue;
            }

            if (isDirectory) {
                (void) dirsToFix.emplace(filePath);
                continue;
            }
            (void) dirsToFix.emplace(filePath.parent_path());
        }

        // Fix parent directories status
        bool ok = true;
        for (const auto &dir: dirsToFix) {
            if (!_connector->vfsProcessDirStatus(dir, _vfsSetupParams.localPath)) {
                LOGW_WARN(logger(), L"Error in vfsProcessDirStatus for " << Utility::formatErrno(dir, errno));
                ok = false;
            }
        }
        return ok ? ExitInfo(ExitCode::Ok) : ExitInfo(ExitCode::SystemError, ExitCause::UnableToCreateVfs);
    }

    return ExitCode::Ok;
}

void VfsMac::stopImpl(bool unregister) {
    LOG_DEBUG(logger(), "stop - syncDbId = " << _vfsSetupParams.syncDbId);

    if (unregister) {
        if (!_connector) {
            LOG_WARN(logger(), "LiteSyncExtConnector not initialized!");
            return;
        }

        if (!_connector->vfsStop(_vfsSetupParams.syncDbId)) {
            LOG_WARN(logger(), "Error in vfsStop!");
            return;
        }
    }
}

void VfsMac::dehydrate(const SyncPath &absoluteFilepath) {
    LOGW_DEBUG(logger(), L"dehydrate - " << Utility::formatSyncPath(absoluteFilepath));

    const auto relativePath = CommonUtility::relativePath(_vfsSetupParams.localPath, absoluteFilepath);

    // Check file status
    if (!_syncFileStatus) {
        LOGW_DEBUG(logger(), L"Unable to check status for file" << Utility::formatSyncPath(absoluteFilepath));
        return;
    }
    SyncFileStatus status = SyncFileStatus::Unknown;
    _syncFileStatus(_vfsSetupParams.syncDbId, relativePath, status);
    if (status == SyncFileStatus::Unknown) {
        // The file is not synchronized, do nothing
        LOGW_DEBUG(logger(), L"Cannot dehydrate an unsynced file with " << Utility::formatSyncPath(absoluteFilepath));
        return;
    }

    // Dehydrate file
    if (!_connector->vfsDehydratePlaceHolder(absoluteFilepath.native(), _vfsSetupParams.localPath)) {
        LOG_WARN(logger(), "Error in vfsDehydratePlaceHolder!");
    }

    _setSyncFileSyncing(_vfsSetupParams.syncDbId, relativePath, false);
}

void VfsMac::hydrate(const SyncPath &absoluteFilepath) {
    LOGW_DEBUG(logger(), L"hydrate - " << Utility::formatSyncPath(absoluteFilepath));

    if (!_connector->vfsHydratePlaceHolder(absoluteFilepath.native())) {
        LOG_WARN(logger(), "Error in vfsHydratePlaceHolder!");
    }

    const auto relativePath = CommonUtility::relativePath(_vfsSetupParams.localPath, absoluteFilepath);
    _setSyncFileSyncing(_vfsSetupParams.syncDbId, relativePath, false);
}

ExitInfo VfsMac::forceStatus(const SyncPath &path, const VfsStatus &vfsStatus) {
    if (ExitInfo exitInfo = checkIfPathIsValid(path, true); !exitInfo) {
        LOGW_WARN(logger(), L"Error in VfsMac::forceStatus: " << exitInfo);
        return exitInfo;
    }

    if (!_connector->vfsSetStatus(path, _vfsSetupParams.localPath, vfsStatus)) {
        LOG_WARN(logger(), "Error in vfsSetStatus!");
        return handleVfsError(path);
    }

    return ExitCode::Ok;
}

bool VfsMac::cleanUpStatuses() {
    return _connector->vfsCleanUpStatuses(_vfsSetupParams.localPath);
}

void VfsMac::clearFileAttributes(const SyncPath &path) {
    _connector->vfsClearFileAttributes(path);
}

ExitInfo VfsMac::updateMetadata(const SyncPath &absoluteFilePath, time_t creationTime, time_t modtime, int64_t size,
                                const NodeId &fileIdStr) {
    Q_UNUSED(fileIdStr);

    if (extendedLog()) {
        LOGW_DEBUG(logger(), L"updateMetadata - " << Utility::formatSyncPath(absoluteFilePath));
    }

    if (!_connector) {
        LOG_WARN(logger(), "LiteSyncExtConnector not initialized!");
        return ExitCode::LogicError;
    }

    struct stat fileStat;
    fileStat.st_size = size;
    fileStat.st_mtimespec = {modtime, 0};
    fileStat.st_atimespec = {modtime, 0};
    fileStat.st_birthtimespec = {creationTime, 0};
    fileStat.st_mode = S_IFREG;

    if (!_connector->vfsUpdateMetadata(absoluteFilePath, &fileStat)) {
        LOG_WARN(logger(), "Error in vfsUpdateMetadata!");
        return handleVfsError(absoluteFilePath);
    }

    return ExitCode::Ok;
}

ExitInfo VfsMac::createPlaceholder(const SyncPath &relativeLocalPath, const SyncFileItem &item) {
    if (extendedLog()) {
        LOGW_DEBUG(logger(), L"createPlaceholder - file = " << Utility::formatSyncPath(relativeLocalPath));
    }

    if (relativeLocalPath.empty()) {
        LOG_WARN(logger(), "VfsMac::createPlaceholder - relativeLocalPath cannot be empty.");
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
    struct stat fileStat;
    fileStat.st_size = item.size();
    fileStat.st_mtimespec = {item.modTime(), 0};
    fileStat.st_atimespec = {item.modTime(), 0};
    fileStat.st_birthtimespec = {item.creationTime(), 0};
    if (item.type() == NodeType::Directory) {
        fileStat.st_mode = S_IFDIR;
    } else {
        fileStat.st_mode = S_IFREG;
    }

    if (!_connector->vfsCreatePlaceHolder(relativeLocalPath.native(), _vfsSetupParams.localPath, &fileStat)) {
        LOG_WARN(logger(), "Error in vfsCreatePlaceHolder!");
        return defaultVfsError(); // handleVfsError is not suitable here, the file doesn't exist, but we don't want to return
                                  // NotFound as it make no sense in the context of a create
    }

    return ExitCode::Ok;
}

ExitInfo VfsMac::dehydratePlaceholder(const SyncPath &relativePath) {
    if (extendedLog()) {
        LOGW_DEBUG(logger(), L"dehydratePlaceholder - file " << Utility::formatSyncPath(relativePath));
    }

    const auto fullPath(_vfsSetupParams.localPath / relativePath);
    if (ExitInfo exitInfo = checkIfPathIsValid(fullPath, true); !exitInfo) {
        return exitInfo;
    }

    // Check if the file is a placeholder
    VfsStatus vfsStatus;
    if (!_connector->vfsGetStatus(fullPath.native(), vfsStatus)) {
        LOG_WARN(logger(), "Error in vfsGetStatus!");
        return handleVfsError(fullPath);
    }

    if (!vfsStatus.isPlaceholder) {
        // Not a placeholder
        LOGW_WARN(logger(), L"Not a placeholder: " << Utility::formatSyncPath(fullPath));
        return {ExitCode::SystemError, ExitCause::NotPlaceHolder};
    }

    if (vfsStatus.isHydrated) {
        LOGW_DEBUG(logger(), L"Dehydrate file with " << Utility::formatSyncPath(fullPath));
        dehydrate(fullPath);
    }

    return ExitCode::Ok;
}

ExitInfo VfsMac::convertToPlaceholder(const SyncPath &absolutePath, const SyncFileItem &item) {
    if (extendedLog()) {
        LOGW_DEBUG(logger(), L"convertToPlaceholder - " << Utility::formatSyncPath(absolutePath));
    }

    if (absolutePath.empty()) {
        LOG_WARN(logger(), "Invalid parameters");
        return {ExitCode::SystemError, ExitCause::InvalidArgument};
    }
    if (const auto exitInfo = checkIfPathIsValid(absolutePath, true); !exitInfo) {
        return exitInfo;
    }

    // Check if the file is already a placeholder
    VfsStatus vfsStatus;
    if (!_connector->vfsGetStatus(absolutePath, vfsStatus)) {
        LOG_WARN(logger(), "Error in vfsGetStatus!");
        return handleVfsError(absolutePath);
    }

    if (!vfsStatus.isPlaceholder) {
        // Convert to placeholder
        if (!_connector->vfsConvertToPlaceHolder(absolutePath.native(), !item.dehydrated())) {
            LOG_WARN(logger(), "Error in vfsConvertToPlaceHolder!");
            return handleVfsError(absolutePath);
        }

        // If item is a directory, also convert items inside it
        ItemType itemType;
        if (!IoHelper::getItemType(absolutePath, itemType)) {
            LOGW_WARN(KDC::Log::instance()->getLogger(),
                      L"Error in IoHelper::getItemType : " << Utility::formatSyncPath(absolutePath).c_str());
            return ExitCode::SystemError;
        }

        if (itemType.ioError == IoError::NoSuchFileOrDirectory) {
            LOGW_DEBUG(KDC::Log::instance()->getLogger(),
                       L"Item does not exist anymore : " << Utility::formatSyncPath(absolutePath).c_str());
            return {ExitCode::SystemError, ExitCause::NotFound};
        }

        if (itemType.ioError == IoError::AccessDenied) {
            LOGW_DEBUG(KDC::Log::instance()->getLogger(),
                       L"Item misses search permission : " << Utility::formatSyncPath(absolutePath).c_str());
            return {ExitCode::SystemError, ExitCause::FileAccessError};
        }

        const bool isLink = itemType.linkType != LinkType::None;
        bool isDirectory = false;
        if (!isLink) {
            isDirectory = itemType.nodeType == NodeType::Directory;
            if (!isDirectory && itemType.ioError != IoError::Success) {
                LOGW_WARN(logger(), L"Failed to check if the path is a directory: "
                                            << Utility::formatIoError(absolutePath, itemType.ioError).c_str());
                return ExitCode::SystemError;
            }
        }

        if (isDirectory) {
            convertDirContentToPlaceholder(Path2QStr(absolutePath), !item.dehydrated());
        }
    }

    return ExitCode::Ok;
}

void VfsMac::convertDirContentToPlaceholder(const QString &dirPath, bool isHydratedIn) {
    try {
        std::error_code ec;
        auto dirIt = std::filesystem::recursive_directory_iterator(
                QStr2Path(dirPath), std::filesystem::directory_options::skip_permission_denied, ec);
        if (ec) {
            LOGW_WARN(logger(), L"Error in convertDirContentToPlaceholder: " << Utility::formatStdError(ec));
            return;
        }
        for (; dirIt != std::filesystem::recursive_directory_iterator(); ++dirIt) {
#if defined(KD_WINDOWS)
            // skip_permission_denied doesn't work on Windows
            try {
                bool dummy = dirIt->exists();
                (void) (dummy);
            } catch (std::filesystem::filesystem_error &) {
                dirIt.disable_recursion_pending();
                continue;
            }
#endif

            const SyncPath absolutePath = dirIt->path();

            // Check if the directory entry is managed
            bool isManaged = true;
            IoError ioError = IoError::Success;
            if (!Utility::checkIfDirEntryIsManaged(*dirIt, isManaged, ioError)) {
                LOGW_WARN(logger(), L"Error in Utility::checkIfDirEntryIsManaged : " << Utility::formatSyncPath(absolutePath));
                dirIt.disable_recursion_pending();
                continue;
            }

            if (ioError == IoError::NoSuchFileOrDirectory) {
                LOGW_DEBUG(logger(), L"Directory entry does not exist anymore : " << Utility::formatSyncPath(absolutePath));
                dirIt.disable_recursion_pending();
                continue;
            }

            if (ioError == IoError::AccessDenied) {
                LOGW_DEBUG(logger(), L"Directory misses search permission : " << Utility::formatSyncPath(absolutePath));
                dirIt.disable_recursion_pending();
                continue;
            }

            if (!isManaged) {
                LOGW_DEBUG(logger(), L"Directory entry is not managed : " << Utility::formatSyncPath(absolutePath));
                dirIt.disable_recursion_pending();
                continue;
            }

            // Check if the file is already a placeholder
            VfsStatus vfsStatus;
            if (!_connector->vfsGetStatus(absolutePath, vfsStatus)) {
                LOG_WARN(logger(), "Error in vfsGetStatus!");
                continue;
            }

            if (!vfsStatus.isPlaceholder) {
                // Convert to placeholder
                if (!_connector->vfsConvertToPlaceHolder(absolutePath, isHydratedIn)) {
                    LOG_WARN(logger(), "Error in vfsConvertToPlaceHolder!");
                }
            }
        }
    } catch (std::filesystem::filesystem_error &e) {
        LOG_WARN(logger(), "Error caught in vfs_mac::convertDirContentToPlaceholder: code=" << e.code() << " error=" << e.what());
    } catch (...) {
        LOG_WARN(logger(), "Error caught in vfs_mac::convertDirContentToPlaceholder");
    }
}

void VfsMac::resetLiteSyncConnector() {
    try {
        _connector->resetConnector(logger(), _vfsSetupParams.executeCommand);
    } catch (const std::runtime_error &) {
        LOG_WARN(logger(), "Error resetting LiteSyncExtConnector");
    }
}

ExitInfo VfsMac::updateFetchStatus(const SyncPath &tmpPath, const SyncPath &path, int64_t received, bool &canceled,
                                   bool &finished) {
    if (extendedLog()) {
        LOGW_INFO(logger(), L"updateFetchStatus file " << Utility::formatSyncPath(path) << L" - " << received);
    }
    if (tmpPath.empty()) {
        LOG_WARN(logger(), "Invalid parameters");
        return {ExitCode::SystemError, ExitCause::InvalidArgument};
    }

    if (ExitInfo exitInfo = checkIfPathIsValid(path, true); !exitInfo) {
        if (exitInfo == ExitInfo(ExitCode::SystemError, ExitCause::NotFound)) {
            return ExitCode::Ok;
        }
        return exitInfo;
    }

    finished = false;
    if (!_connector->vfsUpdateFetchStatus(tmpPath, path, _vfsSetupParams.localPath, static_cast<uint64_t>(received), canceled,
                                          finished)) {
        LOG_WARN(logger(), "Error in vfsUpdateFetchStatus!");
        return handleVfsError(path);
    }

    return ExitCode::Ok;
}

ExitInfo VfsMac::updateFetchStatus(const SyncPath &absolutePath, const std::string &status) {
    if (!_connector->vfsUpdateFetchStatus(absolutePath, status)) {
        LOG_WARN(logger(), "Error in vfsUpdateFetchStatus!");
        return handleVfsError(absolutePath);
    }
    return ExitCode::Ok;
}

void VfsMac::cancelHydrate(const SyncPath &absoluteFilepath) {
    _connector->vfsCancelHydrate(absoluteFilepath);
}

ExitInfo VfsMac::isDehydratedPlaceholder(const SyncPath &relativeFilePath, bool &isDehydrated) {
    const SyncPath absoluteFilePath(_vfsSetupParams.localPath / relativeFilePath);

    VfsStatus vfsStatus;
    if (!_connector->vfsGetStatus(absoluteFilePath, vfsStatus)) {
        LOG_WARN(logger(), "Error in vfsGetStatus!");
        return handleVfsError(absoluteFilePath);
    }
    isDehydrated = !vfsStatus.isHydrated;

    return ExitCode::Ok;
}

ExitInfo VfsMac::setPinState(const SyncPath &relativePath, PinState state) {
    SyncPath fullPath(_vfsSetupParams.localPath / relativePath);

    if (ExitInfo exitInfo = checkIfPathIsValid(fullPath, true); !exitInfo) {
        return exitInfo;
    }

    if (!_connector->vfsSetPinState(
                fullPath, _vfsSetupParams.localPath,
                (state == PinState::AlwaysLocal ? litesync_attrs::pinStatePinned : litesync_attrs::pinStateUnpinned))) {
        LOG_WARN(logger(), "Error in vfsSetPinState!");
        return handleVfsError(fullPath);
    }

    return ExitCode::Ok;
}

PinState VfsMac::pinState(const SyncPath &relativePath) {
    // Read pin state from file attributes
    const auto fullPath(_vfsSetupParams.localPath / relativePath);
    std::string pinState;
    if (!_connector->vfsGetPinState(fullPath, pinState)) {
        return PinState::Unknown;
    }

    if (pinState == litesync_attrs::pinStatePinned) {
        return PinState::AlwaysLocal;
    } else if (pinState == litesync_attrs::pinStateUnpinned) {
        return PinState::OnlineOnly;
    }

    return PinState::Unknown;
}

ExitInfo VfsMac::status(const SyncPath &filePath, VfsStatus &vfsStatus) {
    if (!_connector->vfsGetStatus(filePath, vfsStatus)) {
        LOG_WARN(logger(), "Error in vfsGetStatus!");
        return handleVfsError(filePath);
    }

    return ExitCode::Ok;
}

void VfsMac::exclude(const SyncPath &path) {
    LOGW_DEBUG(logger(), L"exclude - " << Utility::formatSyncPath(path));

    VfsStatus vfsStatus;
    if (!_connector->vfsGetStatus(path.native(), vfsStatus)) {
        LOG_WARN(logger(), "Error in vfsGetStatus!");
        return;
    }

    if (vfsStatus.isSyncing || vfsStatus.isHydrated) {
        vfsStatus.isSyncing = false;
        vfsStatus.isHydrated = true;
        vfsStatus.progress = 0;
        if (!_connector->vfsSetStatus(path.native(), _vfsSetupParams.localPath, vfsStatus)) {
            LOG_WARN(logger(), "Error in vfsSetStatus!");
            return;
        }
    }

    if (vfsStatus.isPlaceholder) {
        std::string pinState;
        if (!_connector->vfsGetPinState(path.native(), pinState)) {
            LOG_WARN(logger(), "Error in vfsGetPinState!");
            return;
        }

        if (pinState != litesync_attrs::pinStateExcluded) {
            if (!_connector->vfsSetPinState(path.native(), _vfsSetupParams.localPath, litesync_attrs::pinStateExcluded)) {
                LOG_WARN(logger(), "Error in vfsSetPinState!");
            }
        }
    }
}

bool VfsMac::isExcluded(const SyncPath &filePath) {
    VfsStatus vfsStatus;
    if (!_connector->vfsGetStatus(filePath.native(), vfsStatus)) {
        LOG_WARN(logger(), "Error in vfsGetStatus!");
        return false;
    }

    if (vfsStatus.isPlaceholder) {
        return _connector->vfsIsExcluded(filePath);
    }

    return false;
}

ExitInfo VfsMac::setThumbnail(const SyncPath &absoluteFilePath, const QPixmap &pixmap) {
    if (!_connector->vfsSetThumbnail(absoluteFilePath, pixmap)) {
        LOG_WARN(logger(), "Error in vfsSetThumbnail!");
        return handleVfsError(absoluteFilePath);
    }

    return ExitCode::Ok;
}

ExitInfo VfsMac::setAppExcludeList() {
    QString appExcludeList;
    _exclusionAppList(appExcludeList);
    if (!_connector->vfsSetAppExcludeList(QStr2Str(appExcludeList))) {
        LOG_WARN(logger(), "Error in vfsSetAppExcludeList!");
        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo VfsMac::getFetchingAppList(QHash<QString, QString> &appTable) {
    std::unordered_map<std::string, std::string, StringHashFunction, std::equal_to<>> tmpTable;
    for (auto it = appTable.begin(); it != appTable.end(); it++) {
        tmpTable.try_emplace(QStr2Str(it.key()), QStr2Str(it.value()));
    }

    if (!_connector->vfsGetFetchingAppList(tmpTable)) {
        LOG_WARN(logger(), "Error in vfsGetFetchingAppList!");
        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

bool VfsMac::fileStatusChanged(const SyncPath &absoluteFilepath, SyncFileStatus status) {
    LOGW_DEBUG(logger(), L"fileStatusChanged - " << Utility::formatSyncPath(absoluteFilepath) << L" - status = " << status);
    if (std::error_code ec; !std::filesystem::exists(absoluteFilepath, ec)) {
        if (ec.value() != 0) {
            LOGW_WARN(logger(), L"Failed to check if path exists : " << Utility::formatStdError(absoluteFilepath, ec));
            return false;
        }
        // New file
        return true;
    }
    SyncPath relativeFilePath = CommonUtility::relativePath(_vfsSetupParams.localPath, absoluteFilepath);

    if (status == SyncFileStatus::Ignored) {
        exclude(absoluteFilepath);
    } else if (status == SyncFileStatus::Success) {
        // Do nothing
    } else if (status == SyncFileStatus::Syncing) {
        ItemType itemType;
        if (!IoHelper::getItemType(absoluteFilepath, itemType)) {
            LOGW_WARN(logger(), L"Error in IoHelper::getItemType : " << Utility::formatSyncPath(absoluteFilepath));
            return false;
        }

        if (itemType.ioError == IoError::NoSuchFileOrDirectory) {
            LOGW_DEBUG(logger(), L"Item does not exist anymore : " << Utility::formatSyncPath(absoluteFilepath));
            return true;
        }

        if (itemType.ioError == IoError::AccessDenied) {
            LOGW_DEBUG(logger(), L"Item misses search permission : " << Utility::formatSyncPath(absoluteFilepath));
            return true;
        }

        bool isLink = itemType.linkType != LinkType::None;
        bool isDirectory = true;
        if (isLink) {
            isDirectory = false;
        } else {
            isDirectory = itemType.nodeType == NodeType::Directory;
            if (!isDirectory && itemType.ioError != IoError::Success) {
                LOGW_WARN(logger(), L"Failed to check if the path is a directory: "
                                            << Utility::formatIoError(absoluteFilepath, itemType.ioError));
                return false;
            }
        }

        if (!isLink && !isDirectory) {
            auto localPinState = pinState(relativeFilePath);
            bool isDehydrated = false;
            if (ExitInfo exitInfo = isDehydratedPlaceholder(relativeFilePath, isDehydrated); !exitInfo) {
                LOGW_WARN(logger(), L"Error in isDehydratedPlaceholder : " << Utility::formatSyncPath(absoluteFilepath) << L" "
                                                                           << exitInfo);
                return false;
            }
            if (localPinState == PinState::OnlineOnly && !isDehydrated) {
                // Add file path to dehydration queue
                _workerInfo[workerDehydration]._mutex.lock();
                _workerInfo[workerDehydration]._queue.push_front(absoluteFilepath);
                _workerInfo[workerDehydration]._mutex.unlock();
                _workerInfo[workerDehydration]._queueWC.wakeOne();
            } else if (localPinState == PinState::AlwaysLocal && isDehydrated) {
                bool syncing;
                _syncFileSyncing(_vfsSetupParams.syncDbId, relativeFilePath, syncing);
                if (!syncing) {
                    // Set hydrating indicator (avoid double hydration)
                    _setSyncFileSyncing(_vfsSetupParams.syncDbId, relativeFilePath, true);

                    // Add file path to hydration queue
                    _workerInfo[workerHydration]._mutex.lock();
                    _workerInfo[workerHydration]._queue.push_front(absoluteFilepath);
                    _workerInfo[workerHydration]._mutex.unlock();
                    _workerInfo[workerHydration]._queueWC.wakeOne();
                }
            }
        }
    } else if (status == SyncFileStatus::Error) {
        // Nothing to do
    }

    return true;
}
} // namespace KDC
