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

#include "vfs_win.h"
#include "common/utility.h"
#include "libcommon/utility/utility.h"
#include "version.h"
#include "config.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/io/filestat.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"

#include <iostream>
#include <unordered_map>
#include <shobjidl_core.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>

#include <log4cplus/loggingmacros.h>

namespace KDC {

const int s_nb_threads[NB_WORKERS] = {5, 5};

std::unordered_map<QString, SyncFileStatus> s_fetchMap;

VfsWin::VfsWin(VfsSetupParams &vfsSetupParams, QObject *parent) : Vfs(vfsSetupParams, parent) {
    // Initialize LiteSync ext connector
    LOG_INFO(logger(), "Initialize LiteSyncExtConnector");

    TraceCbk debugCallback = std::bind(&VfsWin::debugCbk, this, std::placeholders::_1, std::placeholders::_2);

    Utility::setLogger(logger());
    IoHelper::setLogger(logger());

    if (vfsInit(debugCallback, QString(APPLICATION_SHORTNAME).toStdWString().c_str(), (DWORD) _getpid(),
                KDC::CommonUtility::escape(KDRIVE_VERSION_STRING).toStdWString().c_str(),
                QString(APPLICATION_TRASH_URL).toStdWString().c_str()) != S_OK) {
        LOG_WARN(logger(), "Error in vfsInit!");
        throw std::runtime_error("Error in vfsInit!");
        return;
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

VfsWin::~VfsWin() {
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

void VfsWin::debugCbk(TraceLevel level, const wchar_t *msg) {
    switch (level) {
        case TraceLevel::INFO:
            LOGW_INFO(logger(), msg);
            break;
        case TraceLevel::DEBUG:
            LOGW_DEBUG(logger(), msg);
            break;
        case TraceLevel::WARNING:
            LOGW_WARN(logger(), msg);
            break;
        case TraceLevel::_ERROR:
            LOGW_WARN(logger(), msg);
            break;
    }
}

VirtualFileMode VfsWin::mode() const {
    return VirtualFileMode::Win;
}

ExitInfo VfsWin::startImpl(bool &, bool &, bool &) {
    LOG_DEBUG(logger(), "startImpl: syncDbId=" << _vfsSetupParams._syncDbId);

    wchar_t clsid[39] = L"";
    unsigned long clsidSize = sizeof(clsid);
    if (vfsStart(std::to_wstring(_vfsSetupParams._driveId).c_str(), std::to_wstring(_vfsSetupParams._userId).c_str(),
                 std::to_wstring(_vfsSetupParams._syncDbId).c_str(), _vfsSetupParams._localPath.filename().native().c_str(),
                 _vfsSetupParams._localPath.lexically_normal().native().c_str(), clsid, &clsidSize) != S_OK) {
        LOG_WARN(logger(), "Error in vfsStart: syncDbId=" << _vfsSetupParams._syncDbId);
        return {ExitCode::SystemError, ExitCause::UnableToCreateVfs};
    }

    _vfsSetupParams._namespaceCLSID = Utility::ws2s(std::wstring(clsid));

    return ExitCode::Ok;
}

void VfsWin::stopImpl(bool unregister) {
    LOG_DEBUG(logger(), "stop: syncDbId=" << _vfsSetupParams._syncDbId);

    if (vfsStop(std::to_wstring(_vfsSetupParams._driveId).c_str(), std::to_wstring(_vfsSetupParams._syncDbId).c_str(),
                unregister) != S_OK) {
        LOG_WARN(logger(), "Error in vfsStop: syncDbId=" << _vfsSetupParams._syncDbId);
    }

    LOG_DEBUG(logger(), "stop done: syncDbId=" << _vfsSetupParams._syncDbId);
}

void VfsWin::dehydrate(const QString &path) {
    LOGW_DEBUG(logger(), L"dehydrate: " << Utility::formatSyncPath(QStr2Path(path)).c_str());

    // Dehydrate file
    if (vfsDehydratePlaceHolder(QStr2Path(QDir::toNativeSeparators(path)).c_str()) != S_OK) {
        LOGW_WARN(logger(), L"Error in vfsDehydratePlaceHolder: " << Utility::formatSyncPath(QStr2Path(path)).c_str());
    }

    QString relativePath = QStringView(path).mid(_vfsSetupParams._localPath.native().size() + 1).toUtf8();
    _setSyncFileSyncing(_vfsSetupParams._syncDbId, QStr2Path(relativePath), false);
}

void VfsWin::hydrate(const QString &path) {
    LOGW_DEBUG(logger(), L"hydrate: " << Utility::formatSyncPath(QStr2Path(path)).c_str());

    if (vfsHydratePlaceHolder(std::to_wstring(_vfsSetupParams._driveId).c_str(),
                              std::to_wstring(_vfsSetupParams._syncDbId).c_str(),
                              QStr2Path(QDir::toNativeSeparators(path)).c_str()) != S_OK) {
        LOGW_WARN(logger(), L"Error in vfsHydratePlaceHolder: " << Utility::formatSyncPath(QStr2Path(path)).c_str());
    }

    QString relativePath = QStringView(path).mid(_vfsSetupParams._localPath.native().size() + 1).toUtf8();
    _setSyncFileSyncing(_vfsSetupParams._syncDbId, QStr2Path(relativePath), false);
}

void VfsWin::cancelHydrate(const QString &path) {
    LOGW_DEBUG(logger(), L"cancelHydrate: " << Utility::formatSyncPath(QStr2Path(path)).c_str());

    if (vfsCancelFetch(std::to_wstring(_vfsSetupParams._driveId).c_str(), std::to_wstring(_vfsSetupParams._syncDbId).c_str(),
                       QStr2Path(QDir::toNativeSeparators(path)).c_str()) != S_OK) {
        LOGW_WARN(logger(), L"Error in vfsCancelFetch: " << Utility::formatSyncPath(QStr2Path(path)).c_str());
        return;
    }

    QString relativePath = QStringView(path).mid(_vfsSetupParams._localPath.native().size() + 1).toUtf8();
    _setSyncFileSyncing(_vfsSetupParams._syncDbId, QStr2Path(relativePath), false);
}

void VfsWin::exclude(const QString &path) {
    LOGW_DEBUG(logger(), L"exclude: " << Utility::formatSyncPath(QStr2Path(path)).c_str());

    DWORD dwAttrs = GetFileAttributesW(QStr2Path(QDir::toNativeSeparators(path)).c_str());
    if (dwAttrs == INVALID_FILE_ATTRIBUTES) {
        DWORD errorCode = GetLastError();
        LOGW_WARN(logger(),
                  L"Error in GetFileAttributesW: " << Utility::formatSyncPath(QStr2Path(path)).c_str() << L" code=" << errorCode);
        return;
    }

    if (vfsSetPinState(QStr2Path(QDir::toNativeSeparators(path)).c_str(), VFS_PIN_STATE_EXCLUDED) != S_OK) {
        LOGW_WARN(logger(), L"Error in vfsSetPinState: " << Utility::formatSyncPath(QStr2Path(path)).c_str());
        return;
    }
}

void VfsWin::setPlaceholderStatus(const QString &path, bool syncOngoing) {
    if (vfsSetPlaceHolderStatus(QStr2Path(QDir::toNativeSeparators(path)).c_str(), syncOngoing) != S_OK) {
        LOGW_WARN(logger(), L"Error in vfsSetPlaceHolderStatus: " << Utility::formatSyncPath(QStr2Path(path)).c_str());
        return;
    }
}

ExitInfo VfsWin::updateMetadata(const QString &filePath, time_t creationTime, time_t modtime, qint64 size, const QByteArray &,
                                QString *) {
    LOGW_DEBUG(logger(), L"updateMetadata: " << Utility::formatSyncPath(QStr2Path(filePath)).c_str() << L" creationTime="
                                             << creationTime << L" modtime=" << modtime);

    SyncPath fullPath(_vfsSetupParams._localPath / QStr2Path(filePath));
    bool exists = false;
    IoError ioError = IoError::Success;
    if (!IoHelper::checkIfPathExists(fullPath, exists, ioError)) {
        LOGW_WARN(logger(), L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(fullPath, ioError).c_str());
        return ExitCode::SystemError;
    }
    if (ioError == IoError::AccessDenied) {
        LOGW_WARN(logger(), L"UpdateMetadata failed because access is denied: " << Utility::formatSyncPath(fullPath).c_str());
        return {ExitCode::SystemError, ExitCause::FileAccessError};
    }

    if (!exists) {
        LOGW_WARN(logger(), L"File/directory doesn't exists: " << Utility::formatSyncPath(fullPath).c_str());
        return {ExitCode::SystemError, ExitCause::NotFound};
    }

    // Update placeholder
    WIN32_FIND_DATA findData;
    findData.nFileSizeHigh = (DWORD) (size >> 32);
    findData.nFileSizeLow = (DWORD) (size & 0xFFFFFFFF);
    OldUtility::UnixTimeToFiletime(modtime, &findData.ftLastWriteTime);
    findData.ftLastAccessTime = findData.ftLastWriteTime;
    OldUtility::UnixTimeToFiletime(creationTime, &findData.ftCreationTime);
    findData.dwFileAttributes = GetFileAttributesW(QStr2Path(filePath).c_str());

    if (vfsUpdatePlaceHolder(QStr2Path(QDir::toNativeSeparators(filePath)).c_str(), &findData) != S_OK) {
        LOGW_WARN(logger(), L"Error in vfsUpdatePlaceHolder: " << Utility::formatSyncPath(fullPath).c_str());
    }

    return ExitCode::Ok;
}

bool VfsWin::createPlaceholder(const SyncPath &relativeLocalPath, const SyncFileItem &item) {
    LOGW_DEBUG(logger(), L"createPlaceholder: " << Utility::formatSyncPath(relativeLocalPath).c_str());

    if (relativeLocalPath.empty()) {
        LOG_WARN(logger(), "Empty file!");
        return false;
    }

    if (!item.remoteNodeId().has_value()) {
        LOGW_WARN(logger(), L"Empty remote nodeId: " << Utility::formatSyncPath(relativeLocalPath).c_str());
        return false;
    }

    SyncPath fullPath(_vfsSetupParams._localPath / relativeLocalPath);
    bool exists = false;
    IoError ioError = IoError::Success;
    if (!IoHelper::checkIfPathExists(fullPath, exists, ioError)) {
        LOGW_WARN(logger(), L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(fullPath, ioError).c_str());
        return false;
    }

    if (exists) {
        LOGW_WARN(logger(), L"Item already exists: " << Utility::formatSyncPath(fullPath).c_str());
        return false;
    }

    // Create placeholder
    WIN32_FIND_DATA findData;
    findData.nFileSizeHigh = (DWORD) (item.size() >> 32);
    findData.nFileSizeLow = (DWORD) (item.size() & 0xFFFFFFFF);
    OldUtility::UnixTimeToFiletime(item.creationTime(), &findData.ftCreationTime);
    OldUtility::UnixTimeToFiletime(item.modTime(), &findData.ftLastWriteTime);
    findData.ftLastAccessTime = findData.ftLastWriteTime;
    findData.dwFileAttributes = (item.type() == NodeType::Directory ? FILE_ATTRIBUTE_DIRECTORY : 0);

    if (vfsCreatePlaceHolder(Utility::s2ws(item.remoteNodeId().value()).c_str(),
                             relativeLocalPath.lexically_normal().native().c_str(),
                             _vfsSetupParams._localPath.lexically_normal().native().c_str(), &findData) != S_OK) {
        LOGW_WARN(logger(), L"Error in vfsCreatePlaceHolder: " << Utility::formatSyncPath(fullPath).c_str());
    }

    // !!! Creating a placeholder DOESN'T triggers any file system event !!!
    // Setting the pin state triggers an EDIT event and then the insertion into the local snapshot
    if (vfsSetPinState(fullPath.lexically_normal().native().c_str(), VFS_PIN_STATE_UNPINNED) != S_OK) {
        LOGW_WARN(logger(), L"Error in vfsSetPinState: " << Utility::formatSyncPath(fullPath).c_str());
        return false;
    }

    return true;
}

bool VfsWin::dehydratePlaceholder(const QString &path) {
    LOGW_DEBUG(logger(), L"dehydratePlaceholder: " << Utility::formatSyncPath(QStr2Path(path)).c_str());

    if (path.isEmpty()) {
        LOG_WARN(logger(), "Empty file!");
        return false;
    }

    SyncPath fullPath(_vfsSetupParams._localPath / QStr2Path(path));
    bool exists = false;
    IoError ioError = IoError::Success;
    if (!IoHelper::checkIfPathExists(fullPath, exists, ioError)) {
        LOGW_WARN(logger(), L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(fullPath, ioError).c_str());
        return false;
    }

    if (!exists) {
        // File doesn't exist
        LOGW_WARN(logger(), L"File doesn't exist: " << Utility::formatSyncPath(fullPath).c_str());
        return false;
    }

    // Check if the file is a placeholder
    bool isPlaceholder;
    if (vfsGetPlaceHolderStatus(fullPath.lexically_normal().native().c_str(), &isPlaceholder, nullptr, nullptr) != S_OK) {
        LOGW_WARN(logger(), L"Error in vfsGetPlaceHolderStatus: " << Utility::formatSyncPath(fullPath).c_str());
        return false;
    }

    if (!isPlaceholder) {
        // Not a placeholder
        LOGW_WARN(logger(), L"Not a placeholder: " << Utility::formatSyncPath(fullPath).c_str());
        return false;
    }

    LOGW_DEBUG(logger(), L"Dehydrate file: " << Utility::formatSyncPath(fullPath).c_str());
    auto dehydrateFct = [=]() { dehydrate(QString::fromStdWString(fullPath.lexically_normal().native())); };
    std::thread dehydrateTask(dehydrateFct);
    dehydrateTask.detach();

    return true;
}

bool VfsWin::convertToPlaceholder(const QString &path, const SyncFileItem &item) {
    LOGW_DEBUG(logger(), L"convertToPlaceholder: " << Utility::formatSyncPath(QStr2Path(path)).c_str());

    if (path.isEmpty()) {
        LOG_WARN(logger(), "Invalid parameters");
        return false;
    }

    SyncPath fullPath(QStr2Path(path));
    DWORD dwAttrs = GetFileAttributesW(fullPath.lexically_normal().native().c_str());
    if (dwAttrs == INVALID_FILE_ATTRIBUTES) {
        DWORD errorCode = GetLastError();
        LOGW_WARN(logger(),
                  L"Error in GetFileAttributesW: " << Utility::formatSyncPath(fullPath).c_str() << L" code=" << errorCode);
        return false;
    }

    if (dwAttrs & FILE_ATTRIBUTE_DEVICE) {
        LOGW_DEBUG(logger(), L"Not a valid file or directory: " << Utility::formatSyncPath(fullPath).c_str());
        return false;
    }

    // Check if the file is already a placeholder
    bool isPlaceholder;
    if (vfsGetPlaceHolderStatus(fullPath.lexically_normal().native().c_str(), &isPlaceholder, nullptr, nullptr) != S_OK) {
        LOGW_WARN(logger(), L"Error in vfsGetPlaceHolderStatus: " << Utility::formatSyncPath(fullPath).c_str());
        return false;
    }

    if (!item.localNodeId().has_value()) {
        LOGW_WARN(logger(), L"Item has no local ID: " << Utility::formatSyncPath(fullPath).c_str());
        return false;
    }

    if (!isPlaceholder) {
        // Convert to placeholder
        if (vfsConvertToPlaceHolder(Utility::s2ws(item.localNodeId().value()).c_str(),
                                    fullPath.lexically_normal().native().c_str()) != S_OK) {
            LOGW_WARN(logger(), L"Error in vfsConvertToPlaceHolder: " << Utility::formatSyncPath(fullPath).c_str());
            return false;
        }
    }

    return true;
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
        bool exists = false;
        IoError ioError = IoError::Success;
        if (!IoHelper::checkIfPathExists(fullPath, exists, ioError)) {
            LOGW_WARN(logger(), L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(fullPath, ioError).c_str());
            return;
        }

        if (!exists) {
            // File creation and rename
            LOGW_DEBUG(logger(), L"File doesn't exist: " << Utility::formatSyncPath(fullPath).c_str());
            continue;
        }

        // Check if the file is already a placeholder
        bool isPlaceholder = false;
        if (vfsGetPlaceHolderStatus(fullPath.lexically_normal().native().c_str(), &isPlaceholder, nullptr, nullptr) != S_OK) {
            LOGW_WARN(logger(), L"Error in vfsGetPlaceHolderStatus: " << Utility::formatSyncPath(fullPath).c_str());
            break;
        }

        if (!isPlaceholder) {
            FileStat fileStat;
            IoError ioError = IoError::Success;
            if (!IoHelper::getFileStat(fullPath, &fileStat, ioError)) {
                LOGW_WARN(logger(), L"Error in IoHelper::getFileStat: " << Utility::formatIoError(fullPath, ioError).c_str());
                break;
            }

            if (ioError == IoError::NoSuchFileOrDirectory) {
                LOGW_DEBUG(logger(), L"Directory entry does not exist anymore: " << Utility::formatSyncPath(fullPath).c_str());
                continue;
            } else if (ioError == IoError::AccessDenied) {
                LOGW_WARN(logger(),
                          L"Item: " << Utility::formatSyncPath(fullPath).c_str() << L" rejected because access is denied");
                continue;
            }

            // Convert to placeholder
            if (vfsConvertToPlaceHolder(std::to_wstring(fileStat.inode).c_str(), fullPath.lexically_normal().native().c_str()) !=
                S_OK) {
                LOGW_WARN(logger(), L"Error in vfsConvertToPlaceHolder: " << Utility::formatSyncPath(fullPath).c_str());
                break;
            }

            if (tmpInfo.isDir()) {
                convertDirContentToPlaceholder(tmpPath, isHydratedIn);
            }
        }
    }
}

void VfsWin::clearFileAttributes(const QString &path) {
    std::filesystem::path fullPath(QStr2Path(path));
    if (vfsRevertPlaceHolder(fullPath.lexically_normal().native().c_str()) != S_OK) {
        LOGW_WARN(logger(), L"Error in vfsRevertPlaceHolder: " << Utility::formatSyncPath(fullPath).c_str());
    }
}

bool VfsWin::updateFetchStatus(const QString &tmpPath, const QString &path, qint64 received, bool &canceled, bool &finished) {
    Q_UNUSED(finished)

    LOGW_DEBUG(logger(), L"updateFetchStatus: " << Utility::formatSyncPath(QStr2Path(path)).c_str());

    if (tmpPath.isEmpty() || path.isEmpty()) {
        LOG_WARN(logger(), "Invalid parameters");
        return false;
    }

    SyncPath fullTmpPath(QStr2Path(tmpPath));
    SyncPath fullPath(QStr2Path(path));

    bool exists = false;
    IoError ioError = IoError::Success;
    if (!IoHelper::checkIfPathExists(fullPath, exists, ioError)) {
        LOGW_WARN(logger(), L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(fullPath, ioError).c_str());
        return false;
    }

    if (!exists) {
        return true;
    }

    // Check if the file is a placeholder
    bool isPlaceholder;
    if (vfsGetPlaceHolderStatus(fullPath.lexically_normal().native().c_str(), &isPlaceholder, nullptr, nullptr) != S_OK) {
        LOGW_WARN(logger(), L"Error in vfsGetPlaceHolderStatus: " << Utility::formatSyncPath(fullPath).c_str());
        return false;
    }

    auto updateFct = [=](bool &canceled, bool &finished, bool &error) {
        // Update download progress
        if (vfsUpdateFetchStatus(std::to_wstring(_vfsSetupParams._driveId).c_str(),
                                 std::to_wstring(_vfsSetupParams._syncDbId).c_str(), fullPath.lexically_normal().native().c_str(),
                                 fullTmpPath.lexically_normal().native().c_str(), received, &canceled, &finished) != S_OK) {
            LOGW_WARN(logger(), L"Error in vfsUpdateFetchStatus: " << Utility::formatSyncPath(fullPath).c_str());
            error = true;
            return;
        }
    };

    // Launch update in a separate thread
    bool error = false;
    std::thread updateTask(updateFct, std::ref(canceled), std::ref(finished), std::ref(error));
    updateTask.join();

    return !error;
}

bool VfsWin::forceStatus(const QString &absolutePath, bool isSyncing, int, bool) {
    SyncPath stdPath = QStr2Path(absolutePath);

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

    DWORD dwAttrs = GetFileAttributesW(stdPath.native().c_str());
    if (dwAttrs == INVALID_FILE_ATTRIBUTES) {
        DWORD errorCode = GetLastError();
        LOGW_WARN(logger(),
                  L"Error in GetFileAttributesW: " << Utility::formatSyncPath(stdPath).c_str() << L" code=" << errorCode);
        return false;
    }

    if (dwAttrs & FILE_ATTRIBUTE_DEVICE) {
        LOGW_WARN(logger(), L"Not a valid file or directory: " << Utility::formatSyncPath(stdPath).c_str());
        return false;
    }

    bool isPlaceholder = false;
    if (vfsGetPlaceHolderStatus(stdPath.native().c_str(), &isPlaceholder, nullptr, nullptr) != S_OK) {
        LOGW_WARN(logger(), L"Error in vfsGetPlaceHolderStatus: " << Utility::formatSyncPath(stdPath).c_str());
        return false;
    }

    // Some editors (notepad++) seems to remove the file attributes, therfore we need to verify that the file is still a
    // placeholder
    if (!isPlaceholder) {
        FileStat filestat;
        IoError ioError = IoError::Success;
        if (!IoHelper::getFileStat(stdPath, &filestat, ioError)) {
            LOGW_WARN(logger(), L"Error in IoHelper::getFileStat: " << Utility::formatIoError(stdPath, ioError).c_str());
            return false;
        }

        if (ioError == IoError::NoSuchFileOrDirectory) {
            LOGW_DEBUG(logger(), L"Item does not exist anymore: " << Utility::formatSyncPath(stdPath).c_str());
            return true;
        } else if (ioError == IoError::AccessDenied) {
            LOGW_WARN(logger(), L"Item: " << Utility::formatSyncPath(stdPath).c_str() << L" rejected because access is denied");
            return true;
        }

        NodeId localNodeId = std::to_string(filestat.inode);

        // Convert to placeholder
        if (vfsConvertToPlaceHolder(Utility::s2ws(localNodeId).c_str(), stdPath.native().c_str()) != S_OK) {
            LOGW_WARN(logger(), L"Error in vfsConvertToPlaceHolder: " << Utility::formatSyncPath(stdPath).c_str());
            return false;
        }
    }

    // Set status
    LOGW_DEBUG(logger(), L"Setting syncing status to: " << isSyncing << L" for file: "
                                                        << Utility::formatSyncPath(QStr2Path(absolutePath)).c_str());
    setPlaceholderStatus(absolutePath, isSyncing);

    return true;
}

bool VfsWin::isDehydratedPlaceholder(const QString &initFilePath, bool isAbsolutePath /*= false*/) {
    bool isDehydrated;
    SyncPath filePath(isAbsolutePath ? QStr2Path(initFilePath) : _vfsSetupParams._localPath / QStr2Path(initFilePath));

    if (vfsGetPlaceHolderStatus(filePath.lexically_normal().native().c_str(), nullptr, &isDehydrated, nullptr) != S_OK) {
        LOGW_WARN(logger(), L"Error in vfsGetPlaceHolderStatus: " << Utility::formatSyncPath(filePath).c_str());
        return false;
    }

    return isDehydrated;
}

bool VfsWin::setPinState(const QString &relativePath, PinState state) {
    SyncPath fullPath(_vfsSetupParams._localPath / QStr2Path(relativePath));
    DWORD dwAttrs = GetFileAttributesW(fullPath.lexically_normal().native().c_str());
    if (dwAttrs == INVALID_FILE_ATTRIBUTES) {
        DWORD errorCode = GetLastError();
        LOGW_WARN(logger(),
                  L"Error in GetFileAttributesW: " << Utility::formatSyncPath(fullPath).c_str() << L" code=" << errorCode);
        return false;
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
    }

    if (vfsSetPinState(fullPath.lexically_normal().native().c_str(), vfsState) != S_OK) {
        LOGW_WARN(logger(), L"Error in vfsSetPinState: " << Utility::formatSyncPath(fullPath).c_str());
        return false;
    }

    return true;
}

PinState VfsWin::pinState(const QString &relativePath) {
    // TODO: Use vfsGetPinState instead of reading attributes (GetFileAttributesW). In this case return unspecified in case of
    // VFS_PIN_STATE_INHERIT.
    //  Read pin state from file attributes
    SyncPath fullPath(_vfsSetupParams._localPath / QStr2Path(relativePath));
    DWORD dwAttrs = GetFileAttributesW(fullPath.lexically_normal().native().c_str());

    if (dwAttrs == INVALID_FILE_ATTRIBUTES) {
        LOGW_WARN(logger(), L"Invalid attributes for item: " << Utility::formatSyncPath(fullPath).c_str());
    } else {
        if (dwAttrs & FILE_ATTRIBUTE_PINNED) {
            return PinState::AlwaysLocal;
        } else if (dwAttrs & FILE_ATTRIBUTE_UNPINNED) {
            return PinState::OnlineOnly;
        }
    }

    return PinState::Unspecified;
}

bool VfsWin::status(const QString &filePath, bool &isPlaceholder, bool &isHydrated, bool &isSyncing, int &) {
    // Check if the file is a placeholder
    SyncPath fullPath(QStr2Path(filePath));
    bool isDehydrated = false;
    if (vfsGetPlaceHolderStatus(fullPath.lexically_normal().native().c_str(), &isPlaceholder, &isDehydrated, nullptr) != S_OK) {
        LOGW_WARN(logger(), L"Error in vfsGetPlaceHolderStatus: " << Utility::formatSyncPath(fullPath).c_str());
        return false;
    }

    isHydrated = !isDehydrated;
    isSyncing = false;

    return true;
}

bool VfsWin::fileStatusChanged(const QString &path, SyncFileStatus status) {
    LOGW_DEBUG(logger(), L"fileStatusChanged: " << Utility::formatSyncPath(QStr2Path(path)).c_str() << L" status = " << status);

    SyncPath fullPath(QStr2Path(path));
    bool exists = false;
    IoError ioError = IoError::Success;
    if (!IoHelper::checkIfPathExists(fullPath, exists, ioError)) {
        LOGW_WARN(logger(), L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(fullPath, ioError).c_str());
        return false;
    }

    if (!exists) {
        // New file
        return true;
    }

    if (status == SyncFileStatus::Conflict || status == SyncFileStatus::Ignored) {
        exclude(path);
    } else if (status == SyncFileStatus::Success) {
        bool isDirectory = false;
        IoError ioError = IoError::Success;
        if (!IoHelper::checkIfIsDirectory(fullPath, isDirectory, ioError)) {
            LOGW_WARN(logger(), L"Failed to check if path is a directory: " << Utility::formatIoError(fullPath, ioError).c_str());
            return false;
        }

        if (!isDirectory) {
            // File
            QString fileRelativePath = QStringView{path}.mid(_vfsSetupParams._localPath.native().size() + 1).toUtf8();
            bool isDehydrated = isDehydratedPlaceholder(fileRelativePath);
            forceStatus(path, false, 100, !isDehydrated);
        }
    } else if (status == SyncFileStatus::Syncing) {
        bool isDirectory = false;
        IoError ioError = IoError::Success;
        if (!IoHelper::checkIfIsDirectory(fullPath, isDirectory, ioError)) {
            LOGW_WARN(logger(), L"Failed to check if path is a directory: " << Utility::formatIoError(fullPath, ioError).c_str());
            return false;
        }
        if (!isDirectory) {
            // File
            QString fileRelativePath = QStringView{path}.mid(_vfsSetupParams._localPath.native().size() + 1).toUtf8();
            auto localPinState = pinState(fileRelativePath);
            if (localPinState == PinState::OnlineOnly || localPinState == PinState::AlwaysLocal) {
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
        }
    } else if (status == SyncFileStatus::Error) {
        // Nothing to do
    }

    return true;
}

Worker::Worker(VfsWin *vfs, int type, int num, log4cplus::Logger logger) : _vfs(vfs), _type(type), _num(num), _logger(logger) {}

void Worker::start() {
    LOG_DEBUG(logger(), "Worker with type=" << _type << " and num=" << _num << " started");

    WorkerInfo &workerInfo = _vfs->_workerInfo[_type];

    forever {
        workerInfo._mutex.lock();
        while (workerInfo._queue.empty() && !workerInfo._stop) {
            LOG_DEBUG(logger(), "Worker with type=" << _type << " and num=" << _num << " waiting");
            workerInfo._queueWC.wait(&workerInfo._mutex);
        }

        if (workerInfo._stop) {
            workerInfo._mutex.unlock();
            break;
        }

        QString path = workerInfo._queue.back();
        workerInfo._queue.pop_back();
        workerInfo._mutex.unlock();

        LOG_DEBUG(logger(), "Worker with type=" << _type << " and num=" << _num << " working");

        switch (_type) {
            case WORKER_HYDRATION:
                _vfs->hydrate(path);
                break;
            case WORKER_DEHYDRATION:
                _vfs->dehydrate(path);
                break;
        }
    }

    LOG_DEBUG(logger(), "Worker with type=" << _type << " and num=" << _num << " ended");
}

} // namespace KDC
