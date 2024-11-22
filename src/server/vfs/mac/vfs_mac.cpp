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

#include "vfs_mac.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/utility/utility.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>

#include <iostream>

#include <log4cplus/loggingmacros.h>

namespace KDC {

const int s_nb_threads[NB_WORKERS] = {5, 5};

VfsMac::VfsMac(KDC::VfsSetupParams &vfsSetupParams, QObject *parent) :
    Vfs(vfsSetupParams, parent), _localSyncPath{Path2QStr(_vfsSetupParams._localPath)} {
    // Initialize LiteSync ext connector
    LOG_INFO(logger(), "Initialize LiteSyncExtConnector");

    Utility::setLogger(logger());
    IoHelper::setLogger(logger());

    _connector = LiteSyncExtConnector::instance(logger(), vfsSetupParams._executeCommand);
    if (!_connector) {
        LOG_WARN(logger(), "Error in LiteSyncExtConnector::instance");
        throw std::runtime_error("Unable to initialize LiteSyncExtConnector.");
    }

    // Start hydration/dehydration workers
    // !!! Disabled for testing because no QEventLoop !!!
    if (qApp) {
        // Start worker threads
        for (int i = 0; i < NB_WORKERS; i++) {
            for (int j = 0; j < s_nb_threads[i]; j++) {
                QThread *workerThread = new QThread();
                _workerInfo[i]._threadList.append(workerThread);
                Worker *worker = new Worker(this, i, j, logger());
                worker->moveToThread(workerThread);
                connect(workerThread, &QThread::started, worker, &Worker::start);
                connect(workerThread, &QThread::finished, worker, &QObject::deleteLater);
                connect(workerThread, &QThread::finished, workerThread, &QObject::deleteLater);
                workerThread->start();
            }
        }
    }
}

VfsMac::~VfsMac() {
    // Ask worker threads to stop
    for (int i = 0; i < NB_WORKERS; i++) {
        _workerInfo[i]._mutex.lock();
        _workerInfo[i]._stop = true;
        _workerInfo[i]._mutex.unlock();
        _workerInfo[i]._queueWC.wakeAll();
    }

    // Force threads to stop if needed
    for (int i = 0; i < NB_WORKERS; i++) {
        for (QThread *thread: qAsConst(_workerInfo[i]._threadList)) {
            if (thread) {
                thread->quit();
                if (!thread->wait(1000)) {
                    thread->terminate();
                    thread->wait();
                }
            }
        }
    }
}


VirtualFileMode VfsMac::mode() const {
    return VirtualFileMode::Mac;
}

ExitInfo VfsMac::startImpl(bool &installationDone, bool &activationDone, bool &connectionDone) {
    LOG_DEBUG(logger(), "startImpl - syncDbId=" << _vfsSetupParams._syncDbId);

    if (!_connector) {
        LOG_WARN(logger(), "LiteSyncExtConnector not initialized!");
        return {ExitCode::SystemError, ExitCause::UnableToCreateVfs};
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
    QString folderPath = QString::fromStdString(_vfsSetupParams._localPath.native());
    if (!_connector->vfsStart(_vfsSetupParams._syncDbId, folderPath, isPlaceholder, isSyncing)) {
        LOG_WARN(logger(), "Error in vfsStart!");
        resetLiteSyncConnector();
        return {ExitCode::SystemError, ExitCause::UnableToCreateVfs};
    }

    QStringList filesToFix;
    if (isPlaceholder && isSyncing &&
        _connector->checkFilesAttributes(folderPath, _localSyncPath,
                                         filesToFix)) { // Verify that all files/folders are in the correct state
        // Get the directories to fix
        QSet<QString> dirsToFix;
        for (const auto &file: filesToFix) {
            const QFileInfo fileInfo(file);
            if (fileInfo.isDir()) {
                dirsToFix.insert(file);
                continue;
            }
            dirsToFix.insert(fileInfo.dir().absolutePath());
        }

        // Fix parent directories status
        bool ok = true;
        for (const auto &dir: dirsToFix) {
            if (!_connector->vfsProcessDirStatus(dir, _localSyncPath)) {
                LOGW_WARN(logger(), L"Error in vfsProcessDirStatus for " << Utility::formatPath(dir).c_str() << errno);
                ok = false;
            }
        }
        return ok ? ExitInfo(ExitCode::Ok) : ExitInfo(ExitCode::SystemError, ExitCause::UnableToCreateVfs);
    }

    return ExitCode::Ok;
}

void VfsMac::stopImpl(bool unregister) {
    LOG_DEBUG(logger(), "stop - syncDbId = " << _vfsSetupParams._syncDbId);

    if (unregister) {
        if (!_connector) {
            LOG_WARN(logger(), "LiteSyncExtConnector not initialized!");
            return;
        }

        if (!_connector->vfsStop(_vfsSetupParams._syncDbId)) {
            LOG_WARN(logger(), "Error in vfsStop!");
            return;
        }
    }
}

void VfsMac::dehydrate(const QString &absoluteFilepath) {
    LOGW_DEBUG(logger(), L"dehydrate - " << Utility::formatPath(absoluteFilepath).c_str());

    // Dehydrate file

    if (!_connector->vfsDehydratePlaceHolder(QDir::toNativeSeparators(absoluteFilepath), _localSyncPath)) {
        LOG_WARN(logger(), "Error in vfsDehydratePlaceHolder!");
    }

    QString relativePath =
            QStringView(absoluteFilepath).mid(static_cast<qsizetype>(_vfsSetupParams._localPath.string().size())).toUtf8();
    _setSyncFileSyncing(_vfsSetupParams._syncDbId, QStr2Path(relativePath), false);
}

void VfsMac::hydrate(const QString &path) {
    LOGW_DEBUG(logger(), L"hydrate - " << Utility::formatPath(path).c_str());

    if (!_connector->vfsHydratePlaceHolder(QDir::toNativeSeparators(path))) {
        LOG_WARN(logger(), "Error in vfsHydratePlaceHolder!");
    }

    QString relativePath = QStringView(path).mid(static_cast<qsizetype>(_vfsSetupParams._localPath.string().size())).toUtf8();
    _setSyncFileSyncing(_vfsSetupParams._syncDbId, QStr2Path(relativePath), false);
}

bool VfsMac::forceStatus(const QString &path, bool isSyncing, int progress, bool isHydrated /*= false*/) {
    SyncPath stdPath = QStr2Path(path);

    bool exists = false;
    IoError ioError = IoError::Success;
    if (!IoHelper::checkIfPathExists(stdPath, exists, ioError)) {
        LOGW_WARN(logger(), L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(stdPath, ioError).c_str());
        return false;
    }

    if (!exists) {
        // New file
        return true;
    }

    return _connector->vfsSetStatus(path, _localSyncPath, isSyncing, progress, isHydrated);
}

bool VfsMac::cleanUpStatuses() {
    return _connector->vfsCleanUpStatuses(_localSyncPath);
}

void VfsMac::clearFileAttributes(const QString &path) {
    _connector->vfsClearFileAttributes(path);
}

bool VfsMac::isHydrating() const {
    return false;
}

bool VfsMac::updateMetadata(const QString &absoluteFilePath, time_t creationTime, time_t modtime, qint64 size,
                            const QByteArray &fileId, QString *error) {
    Q_UNUSED(fileId);

    if (extendedLog()) {
        LOGW_DEBUG(logger(), L"updateMetadata - " << Utility::formatPath(absoluteFilePath).c_str());
    }

    if (!_connector) {
        LOG_WARN(logger(), "LiteSyncExtConnector not initialized!");
        return false;
    }

    struct stat fileStat;
    fileStat.st_size = size;
    fileStat.st_mtimespec = {modtime, 0};
    fileStat.st_atimespec = {modtime, 0};
    fileStat.st_birthtimespec = {creationTime, 0};
    fileStat.st_mode = S_IFREG;

    if (!_connector->vfsUpdateMetadata(absoluteFilePath, &fileStat, error)) {
        LOG_WARN(logger(), "Error in vfsUpdateMetadata!");
        return false;
    }

    return true;
}

bool VfsMac::createPlaceholder(const SyncPath &relativeLocalPath, const SyncFileItem &item) {
    if (extendedLog()) {
        LOGW_DEBUG(logger(), L"createPlaceholder - file = " << Utility::formatSyncPath(relativeLocalPath).c_str());
    }

    SyncPath fullPath(_vfsSetupParams._localPath / relativeLocalPath);
    std::error_code ec;
    if (std::filesystem::exists(fullPath, ec)) {
        LOGW_WARN(logger(), L"File/directory " << Utility::formatSyncPath(relativeLocalPath).c_str() << L" already exists!");
        return false;
    }

    if (ec) {
        LOGW_WARN(logger(), L"Failed to check if path exists " << Utility::formatStdError(fullPath, ec).c_str());
        return false;
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

    if (!_connector->vfsCreatePlaceHolder(QString::fromStdString(relativeLocalPath.native()), _localSyncPath, &fileStat)) {
        LOG_WARN(logger(), "Error in vfsCreatePlaceHolder!");
        return false;
    }

    return true;
}

bool VfsMac::dehydratePlaceholder(const QString &path) {
    if (extendedLog()) {
        LOGW_DEBUG(logger(), L"dehydratePlaceholder - file " << Utility::formatPath(path).c_str());
    }
    SyncPath fullPath(_vfsSetupParams._localPath / QStr2Path(path));
    std::error_code ec;
    if (!std::filesystem::exists(fullPath, ec)) {
        if (ec.value() != 0) {
            LOGW_WARN(logger(), L"Failed to check if path exists " << Utility::formatSyncPath(fullPath).c_str() << L": "
                                                                   << KDC::Utility::s2ws(ec.message()).c_str() << L" ("
                                                                   << ec.value() << L")");
            return false;
        }

        // File doesn't exist
        LOG_WARN(logger(), "File doesn't exist!");
        return false;
    }

    // Check if the file is a placeholder
    bool isPlaceholder;
    bool isHydrated;
    bool isSyncing;
    int progress;
    if (!_connector->vfsGetStatus(QString::fromStdString(fullPath.native()), isPlaceholder, isHydrated, isSyncing, progress)) {
        LOG_WARN(logger(), "Error in vfsGetStatus!");
        return false;
    }

    if (isHydrated) {
        LOGW_DEBUG(logger(), L"Dehydrate file with " << Utility::formatPath(path).c_str());
        dehydrate(QString::fromStdString(fullPath.string()));
    }

    return true;
}

bool VfsMac::convertToPlaceholder(const QString &path, const SyncFileItem &item) {
    if (extendedLog()) {
        LOGW_DEBUG(logger(), L"convertToPlaceholder - " << Utility::formatPath(path).c_str());
    }

    if (path.isEmpty()) {
        LOG_WARN(logger(), "Invalid parameters");
        return false;
    }

    SyncPath fullPath(QStr2Path(path));

    // Check if the file is already a placeholder
    bool isPlaceholder;
    bool isHydrated;
    bool isSyncing;
    int progress;
    if (!_connector->vfsGetStatus(path, isPlaceholder, isHydrated, isSyncing, progress)) {
        LOG_WARN(logger(), "Error in vfsGetStatus!");
        return false;
    }

    if (!isPlaceholder) {
        // Convert to placeholder
        if (!_connector->vfsConvertToPlaceHolder(QDir::toNativeSeparators(path), !item.dehydrated())) {
            LOG_WARN(logger(), "Error in vfsConvertToPlaceHolder!");
            return false;
        }

        // If item is a directory, also convert items inside it
        ItemType itemType;
        if (!IoHelper::getItemType(fullPath, itemType)) {
            LOGW_WARN(KDC::Log::instance()->getLogger(),
                      L"Error in IoHelper::getItemType : " << Utility::formatSyncPath(fullPath).c_str());
            return false;
        }

        if (itemType.ioError == IoError::NoSuchFileOrDirectory) {
            LOGW_DEBUG(KDC::Log::instance()->getLogger(),
                       L"Item does not exist anymore : " << Utility::formatSyncPath(fullPath).c_str());
            return true;
        }

        if (itemType.ioError == IoError::AccessDenied) {
            LOGW_DEBUG(KDC::Log::instance()->getLogger(),
                       L"Item misses search permission : " << Utility::formatSyncPath(fullPath).c_str());
            return true;
        }

        const bool isLink = itemType.linkType != LinkType::None;
        bool isDirectory = false;
        if (!isLink) {
            isDirectory = itemType.nodeType == NodeType::Directory;
            if (!isDirectory && itemType.ioError != IoError::Success) {
                LOGW_WARN(logger(), L"Failed to check if the path is a directory: "
                                            << Utility::formatIoError(fullPath, itemType.ioError).c_str());
                return false;
            }
        }

        if (isDirectory) {
            convertDirContentToPlaceholder(path, !item.dehydrated());
        }
    }

    return true;
}

void VfsMac::convertDirContentToPlaceholder(const QString &dirPath, bool isHydratedIn) {
    try {
        std::error_code ec;
        auto dirIt = std::filesystem::recursive_directory_iterator(
                QStr2Path(dirPath), std::filesystem::directory_options::skip_permission_denied, ec);
        if (ec) {
            LOGW_WARN(logger(), L"Error in convertDirContentToPlaceholder: " << Utility::formatStdError(ec).c_str());
            return;
        }
        for (; dirIt != std::filesystem::recursive_directory_iterator(); ++dirIt) {
#ifdef _WIN32
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
            bool isLink = false;
            IoError ioError = IoError::Success;
            if (!Utility::checkIfDirEntryIsManaged(dirIt, isManaged, isLink, ioError)) {
                LOGW_WARN(logger(),
                          L"Error in Utility::checkIfDirEntryIsManaged : " << Utility::formatSyncPath(absolutePath).c_str());
                dirIt.disable_recursion_pending();
                continue;
            }

            if (ioError == IoError::NoSuchFileOrDirectory) {
                LOGW_DEBUG(logger(),
                           L"Directory entry does not exist anymore : " << Utility::formatSyncPath(absolutePath).c_str());
                dirIt.disable_recursion_pending();
                continue;
            }

            if (ioError == IoError::AccessDenied) {
                LOGW_DEBUG(logger(), L"Directory misses search permission : " << Utility::formatSyncPath(absolutePath).c_str());
                dirIt.disable_recursion_pending();
                continue;
            }

            if (!isManaged) {
                LOGW_DEBUG(logger(), L"Directory entry is not managed : " << Utility::formatSyncPath(absolutePath).c_str());
                dirIt.disable_recursion_pending();
                continue;
            }

            // Check if the file is already a placeholder
            bool isPlaceholder;
            bool isHydrated;
            bool isSyncing;
            int progress;
            if (!_connector->vfsGetStatus(Path2QStr(absolutePath), isPlaceholder, isHydrated, isSyncing, progress)) {
                LOG_WARN(logger(), "Error in vfsGetStatus!");
                continue;
            }

            if (!isPlaceholder) {
                // Convert to placeholder
                if (!_connector->vfsConvertToPlaceHolder(Path2QStr(absolutePath), isHydratedIn)) {
                    LOG_WARN(logger(), "Error in vfsConvertToPlaceHolder!");
                    continue;
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
        _connector->resetConnector(logger(), _vfsSetupParams._executeCommand);
    } catch (const std::runtime_error &) {
        LOG_WARN(logger(), "Error resetting LiteSyncExtConnector");
    }
}

bool VfsMac::updateFetchStatus(const QString &tmpPath, const QString &path, qint64 received, bool &canceled, bool &finished) {
    if (extendedLog()) {
        LOGW_INFO(logger(), L"updateFetchStatus file " << Utility::formatPath(path).c_str() << L" - " << received);
    }
    if (tmpPath.isEmpty() || path.isEmpty()) {
        LOG_WARN(logger(), "Invalid parameters");
        return false;
    }

    std::filesystem::path fullPath(QStr2Path(path));
    std::error_code ec;
    if (!std::filesystem::exists(fullPath, ec)) {
        if (ec.value() != 0) {
            LOGW_WARN(logger(), L"Failed to check if path exists : " << Utility::formatSyncPath(fullPath).c_str() << L": "
                                                                     << Utility::s2ws(ec.message()).c_str() << L" (" << ec.value()
                                                                     << L")");
            return false;
        }
        return true;
    }

    // Check if the file is a placeholder
    bool isPlaceholder = false;
    bool isHydrated = false;
    bool isSyncing = false;
    int progress = 0;
    if (!_connector->vfsGetStatus(Path2QStr(fullPath), isPlaceholder, isHydrated, isSyncing, progress)) {
        LOG_WARN(logger(), "Error in vfsGetStatus!");
        return false;
    }

    std::filesystem::path tmpFullPath(QStr2Path(tmpPath));
    auto updateFct = [=, this](bool &canceled, bool &finished, bool &error) {
        // Update download progress
        finished = false;
        if (!_connector->vfsUpdateFetchStatus(Path2QStr(tmpFullPath), Path2QStr(fullPath), _localSyncPath,
                                              static_cast<uint64_t>(received), canceled, finished)) {
            LOG_WARN(logger(), "Error in vfsUpdateFetchStatus!");
            error = true;
            return;
        }

        if (finished) {
            // Do nothing
        }
    };

    // Launch update in a separate thread
    bool error = false;
    updateFct(canceled, finished, error);

    return !error;
}

void VfsMac::cancelHydrate(const QString &filePath) {
    _connector->vfsCancelHydrate(filePath);
}

bool VfsMac::isDehydratedPlaceholder(const QString &initFilePath, bool isAbsolutePath /*= false*/) {
    SyncPath filePath(isAbsolutePath ? QStr2Path(initFilePath) : _vfsSetupParams._localPath / QStr2Path(initFilePath));

    bool isPlaceholder = false;
    bool isHydrated = false;
    bool isSyncing = false;
    int progress = 0;
    if (!_connector->vfsGetStatus(Path2QStr(filePath), isPlaceholder, isHydrated, isSyncing, progress)) {
        LOG_WARN(logger(), "Error in vfsGetStatus!");
        return false;
    }

    return !isHydrated;
}

bool VfsMac::setPinState(const QString &fileRelativePath, PinState state) {
    std::filesystem::path fullPath(_vfsSetupParams._localPath / QStr2Path(fileRelativePath));

    bool exists = false;
    IoError ioError = IoError::Success;
    if (!IoHelper::checkIfPathExists(fullPath, exists, ioError)) {
        LOGW_WARN(logger(), L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(fullPath, ioError).c_str());
        return false;
    }

    if (!exists) {
        // New file
        LOGW_DEBUG(logger(), L"Item does not exist : " << Utility::formatSyncPath(fullPath).c_str());
        return true;
    }

    const QString strPath = Path2QStr(fullPath);
    if (!_connector->vfsSetPinState(strPath, _localSyncPath,
                                    (state == PinState::AlwaysLocal ? VFS_PIN_STATE_PINNED : VFS_PIN_STATE_UNPINNED))) {
        LOG_WARN(logger(), "Error in vfsSetPinState!");
        return false;
    }

    return true;
}

PinState VfsMac::pinState(const QString &relativePath) {
    // Read pin state from file attributes
    SyncPath fullPath(_vfsSetupParams._localPath / QStr2Path(relativePath));
    QString pinState;
    if (!_connector->vfsGetPinState(Path2QStr(fullPath), pinState)) {
        return PinState::Unspecified;
    }

    if (pinState == VFS_PIN_STATE_PINNED) {
        return PinState::AlwaysLocal;
    } else if (pinState == VFS_PIN_STATE_UNPINNED) {
        return PinState::OnlineOnly;
    }

    return PinState::Unspecified;
}

bool VfsMac::status(const QString &filePath, bool &isPlaceholder, bool &isHydrated, bool &isSyncing, int &progress) {
    SyncPath fullPath(QStr2Path(filePath));
    if (!_connector->vfsGetStatus(Path2QStr(fullPath), isPlaceholder, isHydrated, isSyncing, progress)) {
        LOG_WARN(logger(), "Error in vfsGetStatus!");
        return false;
    }

    return true;
}

void VfsMac::exclude(const QString &path) {
    LOGW_DEBUG(logger(), L"exclude - " << Utility::formatPath(path).c_str());

    bool isPlaceholder = false;
    bool isHydrated = false;
    bool isSyncing = false;
    int progress = 0;
    if (!_connector->vfsGetStatus(QDir::toNativeSeparators(path), isPlaceholder, isHydrated, isSyncing, progress)) {
        LOG_WARN(logger(), "Error in vfsGetStatus!");
        return;
    }

    if (isSyncing || isHydrated) {
        if (!_connector->vfsSetStatus(QDir::toNativeSeparators(path), _localSyncPath, false, 0, true)) {
            LOG_WARN(logger(), "Error in vfsSetStatus!");
            return;
        }
    }

    if (isPlaceholder) {
        QString pinState;
        if (!_connector->vfsGetPinState(QDir::toNativeSeparators(path), pinState)) {
            LOG_WARN(logger(), "Error in vfsGetPinState!");
            return;
        }

        if (pinState != VFS_PIN_STATE_EXCLUDED) {
            if (!_connector->vfsSetPinState(QDir::toNativeSeparators(path), _localSyncPath, VFS_PIN_STATE_EXCLUDED)) {
                LOG_WARN(logger(), "Error in vfsSetPinState!");
                return;
            }
        }
    }
}

bool VfsMac::isExcluded(const QString &filePath) {
    bool isPlaceholder;
    bool isHydrated;
    bool isSyncing;
    int progress;
    if (!_connector->vfsGetStatus(QDir::toNativeSeparators(filePath), isPlaceholder, isHydrated, isSyncing, progress)) {
        LOG_WARN(logger(), "Error in vfsGetStatus!");
        return false;
    }

    if (isPlaceholder) {
        return _connector->vfsIsExcluded(filePath);
    }

    return false;
}

bool VfsMac::setThumbnail(const QString &absoluteFilePath, const QPixmap &pixmap) {
    if (!_connector->vfsSetThumbnail(absoluteFilePath, pixmap)) {
        LOG_WARN(logger(), "Error in vfsSetThumbnail!");
        return false;
    }

    return true;
}

bool VfsMac::setAppExcludeList() {
    QString appExcludeList;
    _exclusionAppList(appExcludeList);
    if (!_connector->vfsSetAppExcludeList(appExcludeList)) {
        LOG_WARN(logger(), "Error in vfsSetAppExcludeList!");
        return false;
    }

    return true;
}

bool VfsMac::getFetchingAppList(QHash<QString, QString> &appTable) {
    if (!_connector->vfsGetFetchingAppList(appTable)) {
        LOG_WARN(logger(), "Error in vfsGetFetchingAppList!");
        return false;
    }

    return true;
}

bool VfsMac::fileStatusChanged(const QString &path, SyncFileStatus status) {
    LOGW_DEBUG(logger(), L"fileStatusChanged - " << Utility::formatPath(path).c_str() << L" - status = " << status);

    SyncPath fullPath(QStr2Path(path));
    std::error_code ec;
    if (!std::filesystem::exists(fullPath, ec)) {
        if (ec.value() != 0) {
            LOGW_WARN(logger(), L"Failed to check if path exists : " << Utility::formatSyncPath(fullPath).c_str() << L": "
                                                                     << Utility::s2ws(ec.message()).c_str() << L" (" << ec.value()
                                                                     << L")");
            return false;
        }
        // New file
        return true;
    }

    if (status == SyncFileStatus::Ignored) {
        exclude(QString::fromStdString(fullPath.native()));
    } else if (status == SyncFileStatus::Success) {
        // Do nothing
    } else if (status == SyncFileStatus::Syncing) {
        ItemType itemType;
        if (!IoHelper::getItemType(fullPath, itemType)) {
            LOGW_WARN(logger(), L"Error in IoHelper::getItemType : " << Utility::formatSyncPath(fullPath).c_str());
            return false;
        }

        if (itemType.ioError == IoError::NoSuchFileOrDirectory) {
            LOGW_DEBUG(logger(), L"Item does not exist anymore : " << Utility::formatSyncPath(fullPath).c_str());
            return true;
        }

        if (itemType.ioError == IoError::AccessDenied) {
            LOGW_DEBUG(logger(), L"Item misses search permission : " << Utility::formatSyncPath(fullPath).c_str());
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
                                            << Utility::formatIoError(fullPath, itemType.ioError).c_str());
                return false;
            }
        }

        if (!isLink && !isDirectory) {
            QString fileRelativePath =
                    QStringView(path).mid(static_cast<qsizetype>(_vfsSetupParams._localPath.string().size())).toUtf8();
            auto localPinState = pinState(fileRelativePath);
            bool isDehydrated = isDehydratedPlaceholder(fileRelativePath);
            if (localPinState == PinState::OnlineOnly && !isDehydrated) {
                // Add file path to dehydration queue
                _workerInfo[WORKER_DEHYDRATION]._mutex.lock();
                _workerInfo[WORKER_DEHYDRATION]._queue.push_front(path);
                _workerInfo[WORKER_DEHYDRATION]._mutex.unlock();
                _workerInfo[WORKER_DEHYDRATION]._queueWC.wakeOne();
            } else if (localPinState == PinState::AlwaysLocal && isDehydrated) {
                bool syncing;
                _syncFileSyncing(_vfsSetupParams._syncDbId, QStr2Path(fileRelativePath), syncing);
                if (!syncing) {
                    // Set hydrating indicator (avoid double hydration)
                    _setSyncFileSyncing(_vfsSetupParams._syncDbId, QStr2Path(fileRelativePath), true);

                    // Add file path to hydration queue
                    _workerInfo[WORKER_HYDRATION]._mutex.lock();
                    _workerInfo[WORKER_HYDRATION]._queue.push_front(path);
                    _workerInfo[WORKER_HYDRATION]._mutex.unlock();
                    _workerInfo[WORKER_HYDRATION]._queueWC.wakeOne();
                }
            }
        }
    } else if (status == SyncFileStatus::Error) {
        // Nothing to do
    }

    return true;
}

Worker::Worker(VfsMac *vfs, int type, int num, log4cplus::Logger logger) : _vfs(vfs), _type(type), _num(num), _logger(logger) {}

void Worker::start() {
    LOG_DEBUG(logger(), "Worker " << _type << " - " << _num << " started");

    WorkerInfo &workerInfo = _vfs->_workerInfo[_type];

    forever {
        workerInfo._mutex.lock();
        while (workerInfo._queue.empty() && !workerInfo._stop) {
            LOG_DEBUG(logger(), "Worker " << _type << " - " << _num << " waiting");
            workerInfo._queueWC.wait(&workerInfo._mutex);
        }

        if (workerInfo._stop) {
            workerInfo._mutex.unlock();
            break;
        }

        QString path = workerInfo._queue.back();
        workerInfo._queue.pop_back();
        workerInfo._mutex.unlock();

        LOG_DEBUG(logger(), "Worker " << _type << " - " << _num << " working");

        switch (_type) {
            case WORKER_HYDRATION:
                _vfs->hydrate(path);
                break;
            case WORKER_DEHYDRATION:
                _vfs->dehydrate(path);
                break;
        }
    }

    LOG_DEBUG(logger(), "Worker " << _type << " - " << _num << " ended");
}

} // namespace KDC
