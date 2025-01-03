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

#include "vfs.h"
#include "plugin.h"
#include "version.h"
#include "utility/types.h"
#include "libcommonserver/utility/utility.h" // Path2WStr
#include "libcommonserver/io/iohelper.h"

#include <QOperatingSystemVersion>
#include <QPluginLoader>
#include <QDir>
#include <QFileInfo>

#define MIN_WINDOWS10_MICROVERSION_FOR_CFAPI 16299 // Windows 10 version 1709

using namespace KDC;

Vfs::Vfs(const VfsSetupParams &vfsSetupParams, QObject *parent) :
    QObject(parent), _vfsSetupParams(vfsSetupParams), _extendedLog(false), _started(false) {}

void Vfs::starVfsWorkers() {
    // Start hydration/dehydration workers
    // !!! Disabled for testing because no QEventLoop !!!
    if (qApp) {
        // Start worker threads
        for (int i = 0; i < nbWorkers; i++) {
            for (int j = 0; j < s_nb_threads[i]; j++) {
                auto *workerThread = new QtLoggingThread();
                _workerInfo[i]._threadList.append(workerThread);
                auto *worker = new VfsWorker(this, i, j, logger());
                worker->moveToThread(workerThread);
                connect(workerThread, &QThread::started, worker, &VfsWorker::start);
                connect(workerThread, &QThread::finished, worker, &QObject::deleteLater);
                connect(workerThread, &QThread::finished, workerThread, &QObject::deleteLater);
                workerThread->start();
            }
        }
    }
}

Vfs::~Vfs() {
    // Ask worker threads to stop
    for (auto &worker: _workerInfo) {
        worker._mutex.lock();
        worker._stop = true;
        worker._mutex.unlock();
        worker._queueWC.wakeAll();
    }

    // Force threads to stop if needed
    for (auto &worker: _workerInfo) {
        for (QThread *thread: qAsConst(worker._threadList)) {
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

QString Vfs::modeToString(KDC::VirtualFileMode virtualFileMode) {
    // Note: Strings are used for config and must be stable
    switch (virtualFileMode) {
        case KDC::VirtualFileMode::Off:
            return QStringLiteral("off");
        case KDC::VirtualFileMode::Suffix:
            return QStringLiteral("suffix");
        case KDC::VirtualFileMode::Win:
            return QStringLiteral("wincfapi");
        case KDC::VirtualFileMode::Mac:
            return QStringLiteral("mac");
    }
    return QStringLiteral("off");
}

KDC::VirtualFileMode Vfs::modeFromString(const QString &str) {
    // Note: Strings are used for config and must be stable
    if (str == "off") {
        return KDC::VirtualFileMode::Off;
    } else if (str == "suffix") {
        return KDC::VirtualFileMode::Suffix;
    } else if (str == "wincfapi") {
        return KDC::VirtualFileMode::Win;
    } else if (str == "mac") {
        return KDC::VirtualFileMode::Mac;
    }

    return {};
}

ExitInfo Vfs::start(bool &installationDone, bool &activationDone, bool &connectionDone) {
    if (!_started) {
        ExitInfo exitInfo = startImpl(installationDone, activationDone, connectionDone);
        _started = exitInfo.code() == ExitCode::Ok;
        return exitInfo;
    }
    return ExitCode::Ok;
}

void Vfs::stop(bool unregister) {
    if (_started) {
        stopImpl(unregister);
        _started = false;
    }
}

ExitInfo Vfs::handleVfsError(const SyncPath &itemPath, const SourceLocation location) const {
    if (ExitInfo exitInfo = checkIfPathExists(itemPath, true, location); !exitInfo) {
        return exitInfo;
    }
    return defaultVfsError(location);
}

ExitInfo Vfs::checkIfPathExists(const SyncPath &itemPath, bool shouldExist, const SourceLocation location) const {
    if (itemPath.empty()) {
        LOGW_WARN(logger(), L"Empty path");
        assert(false && "Empty path in a VFS call");
        return {ExitCode::SystemError, ExitCause::NotFound, location};
    }
    bool exists = false;
    IoError ioError = IoError::Unknown;
    if (!IoHelper::checkIfPathExists(itemPath, exists, ioError)) {
        LOGW_WARN(logger(), L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(itemPath, ioError));
        return {ExitCode::SystemError, location};
    }
    if (ioError == IoError::AccessDenied) {
        LOGW_WARN(logger(), L"File access error: " << Utility::formatIoError(itemPath, ioError));
        return {ExitCode::SystemError, ExitCause::FileAccessError, location};
    }
    if (exists != shouldExist) {
        if (shouldExist) {
            LOGW_DEBUG(logger(), L"File doesn't exist: " << Utility::formatSyncPath(itemPath));
            return {ExitCode::SystemError, ExitCause::NotFound, location};
        } else {
            LOGW_DEBUG(logger(), L"File already exists: " << Utility::formatSyncPath(itemPath));
            return {ExitCode::SystemError, ExitCause::FileAlreadyExist, location};
        }
    }
    return ExitCode::Ok;
}

VfsWorker::VfsWorker(Vfs *vfs, int type, int num, log4cplus::Logger logger) :
    _vfs(vfs), _type(type), _num(num), _logger(logger) {}

void VfsWorker::start() {
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
            case workerHydration:
                _vfs->hydrate(path);
                break;
            case workerDehydration:
                _vfs->dehydrate(path);
                break;
            default:
                LOG_ERROR(logger(), "Unknown vfs worker type=" << _type);
                break;
        }
    }

    LOG_DEBUG(logger(), "Worker with type=" << _type << " and num=" << _num << " ended");
}

VfsOff::VfsOff(QObject *parent) : Vfs(VfsSetupParams(), parent) {}

VfsOff::VfsOff(VfsSetupParams &vfsSetupParams, QObject *parent) : Vfs(vfsSetupParams, parent) {}

VfsOff::~VfsOff() {}

ExitInfo VfsOff::forceStatus(const SyncPath &pathStd, bool isSyncing, int /*progress*/, bool /*isHydrated*/) {
    QString path = SyncName2QStr(pathStd.native());
    KDC::SyncPath fullPath(_vfsSetupParams._localPath / QStr2Path(path));
    if (ExitInfo exitInfo = checkIfPathExists(fullPath, true); !exitInfo) {
        return exitInfo;
    }
    // Update Finder
    LOGW_DEBUG(logger(), L"Send status to the Finder extension for file/directory " << Path2WStr(fullPath).c_str());
    QString status = isSyncing ? "SYNC" : "OK";
    _vfsSetupParams._executeCommand(QString("STATUS:%1:%2").arg(status, path).toStdString().c_str());

    return ExitCode::Ok;
}

ExitInfo VfsOff::startImpl(bool &, bool &, bool &) {
    return ExitCode::Ok;
}

static QString modeToPluginName(KDC::VirtualFileMode virtualFileMode) {
    if (virtualFileMode == KDC::VirtualFileMode::Suffix) return "suffix";
    if (virtualFileMode == KDC::VirtualFileMode::Win) return "win";
    if (virtualFileMode == KDC::VirtualFileMode::Mac) return "mac";
    return QString();
}

bool KDC::isVfsPluginAvailable(KDC::VirtualFileMode virtualFileMode, QString &error) {
    if (virtualFileMode == KDC::VirtualFileMode::Off) return true;

    if (virtualFileMode == KDC::VirtualFileMode::Suffix) {
        return false;
    }

    if (virtualFileMode == KDC::VirtualFileMode::Win) {
        if (QOperatingSystemVersion::current().currentType() == QOperatingSystemVersion::OSType::Windows &&
            QOperatingSystemVersion::current() >= QOperatingSystemVersion::Windows10 &&
            QOperatingSystemVersion::current().microVersion() >= MIN_WINDOWS10_MICROVERSION_FOR_CFAPI) {
            return true;
        } else {
            return false;
        }
    }

    if (virtualFileMode == KDC::VirtualFileMode::Mac) {
        if (QOperatingSystemVersion::current().currentType() == QOperatingSystemVersion::OSType::MacOS &&
            QOperatingSystemVersion::current() >= QOperatingSystemVersion::MacOSCatalina) {
            return true;
        } else {
            return false;
        }
    }

    auto name = modeToPluginName(virtualFileMode);
    if (name.isEmpty()) return false;
    auto pluginPath = pluginFileName("vfs", name);
    QPluginLoader loader(pluginPath);

    auto basemeta = loader.metaData();
    if (basemeta.isEmpty() || !basemeta.contains("IID")) {
        error = "Plugin doesn't exist:" + pluginPath;
        return false;
    }
    if (basemeta["IID"].toString() != "org.kdrive.PluginFactory") {
        error = "Plugin has wrong IID:" + pluginPath + " - " + basemeta["IID"].toString();
        return false;
    }

    auto metadata = basemeta["MetaData"].toObject();
    if (metadata["type"].toString() != "vfs") {
        error = "Plugin has wrong type:" + pluginPath + " - " + metadata["type"].toString();
        return false;
    }
    if (metadata["version"].toString() != KDRIVE_VERSION_STRING) {
        error = "Plugin has wrong type:" + pluginPath + " - " + metadata["version"].toString();
        return false;
    }

    // Attempting to load the plugin is essential as it could have dependencies that
    // can't be resolved and thus not be available after all.
    if (!loader.load()) {
        error = "Plugin failed to load:" + pluginPath + " - " + loader.errorString();
        return false;
    }

    return true;
}

KDC::VirtualFileMode KDC::bestAvailableVfsMode() {
    QString error;
    if (isVfsPluginAvailable(KDC::VirtualFileMode::Win, error)) {
        return KDC::VirtualFileMode::Win;
    } else if (isVfsPluginAvailable(KDC::VirtualFileMode::Mac, error)) {
        return KDC::VirtualFileMode::Mac;
    } else if (isVfsPluginAvailable(KDC::VirtualFileMode::Suffix, error)) {
        return KDC::VirtualFileMode::Suffix;
    }
    return KDC::VirtualFileMode::Off;
}

std::unique_ptr<Vfs> KDC::createVfsFromPlugin(KDC::VirtualFileMode virtualFileMode, VfsSetupParams &vfsSetupParams,
                                              QString &error) {
    if (virtualFileMode == KDC::VirtualFileMode::Off) {
        return std::unique_ptr<Vfs>(new VfsOff(vfsSetupParams));
    }

    auto name = modeToPluginName(virtualFileMode);
    if (name.isEmpty()) {
        return nullptr;
    }

    auto pluginPath = pluginFileName("vfs", name);

    if (!isVfsPluginAvailable(virtualFileMode, error)) {
        return nullptr;
    }

    QPluginLoader loader(pluginPath);
    auto plugin = loader.instance();
    if (!plugin) {
        error = "Could not load plugin:" + pluginPath + " - " + loader.errorString();
        return nullptr;
    }

    auto factory = qobject_cast<PluginFactory *>(plugin);
    if (!factory) {
        error = "Plugin: " + pluginPath + " does not implement PluginFactory";
        return nullptr;
    }

    std::unique_ptr<Vfs> vfs;
    try {
        vfs = std::unique_ptr<Vfs>(qobject_cast<Vfs *>(factory->create(vfsSetupParams)));
    } catch (const std::exception &e) {
        error = "Error creating VFS instance from plugin=" + pluginPath + " error=" + e.what();
        return nullptr;
    }

    if (!vfs) {
        error = "Error creating VFS instance from plugin=" + pluginPath;
        return nullptr;
    }

    return vfs;
}
