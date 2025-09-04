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

#include "vfs.h"
#include "vfsworker.h"
#include "plugin.h"
#include "version.h"
#include "utility/types.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/utility/utility.h" // Path2WStr
#include "libcommonserver/io/iohelper.h"

#include <QOperatingSystemVersion>
#include <QPluginLoader>

#define MIN_WINDOWS10_MICROVERSION_FOR_CFAPI 16299 // Windows 10 version 1709

using namespace KDC;

Vfs::Vfs(const VfsSetupParams &vfsSetupParams, QObject *parent) :
    QObject(parent),
    _vfsSetupParams(vfsSetupParams),
    _extendedLog(false),
    _started(false) {}

void Vfs::starVfsWorkers() {
    // Start hydration/dehydration workers
    // !!! Disabled for testing because no QEventLoop !!!
    if (qApp) {
        // Start worker threads
        for (size_t i = 0; i < nbWorkers; i++) {
            for (size_t j = 0; j < s_nb_threads[i]; j++) {
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
}

CommString Vfs::modeToString(KDC::VirtualFileMode virtualFileMode) {
    // Note: Strings are used for config and must be stable
    switch (virtualFileMode) {
        case KDC::VirtualFileMode::Off:
            return Str("off");
        case KDC::VirtualFileMode::Suffix:
            return Str("suffix");
        case KDC::VirtualFileMode::Win:
            return Str("wincfapi");
        case KDC::VirtualFileMode::Mac:
            return Str("mac");
        case KDC::VirtualFileMode::EnumEnd: {
            assert(false && "Invalid enum value in switch statement.");
        }
    }

    return Str("off");
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

ExitInfo Vfs::handleVfsError(const SyncPath &itemPath, const SourceLocation &location) const {
    if (ExitInfo exitInfo = checkIfPathIsValid(itemPath, true, location); !exitInfo) {
        return exitInfo;
    }
    return defaultVfsError(location);
}

ExitInfo Vfs::checkIfPathIsValid(const SyncPath &itemPath, bool shouldExist, const SourceLocation &location) const {
    if (itemPath.empty()) {
        LOGW_WARN(logger(), L"Empty path provided in Vfs::checkIfPathIsValid");
        assert(false && "Empty path in a VFS call");
        return {ExitCode::SystemError, ExitCause::InvalidArgument, location};
    }

    bool exists = false;
    IoError ioError = IoError::Unknown;
    if (!IoHelper::checkIfPathExists(itemPath, exists, ioError)) {
        LOGW_WARN(logger(), L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(itemPath, ioError));
        return {ExitCode::SystemError};
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
            return {ExitCode::SystemError, ExitCause::FileExists, location};
        }
    }
    return ExitCode::Ok;
}

VfsWorker::VfsWorker(Vfs *vfs, size_t type, size_t num, log4cplus::Logger logger) :
    _vfs(vfs),
    _type(type),
    _num(num),
    _logger(logger) {}

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

        SyncPath path = workerInfo._queue.back().native();
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

VfsOff::VfsOff(QObject *parent) :
    Vfs(VfsSetupParams(), parent) {}

VfsOff::VfsOff(const VfsSetupParams &vfsSetupParams, QObject *parent) :
    Vfs(vfsSetupParams, parent) {}

VfsOff::~VfsOff() = default;

ExitInfo VfsOff::forceStatus(const SyncPath &pathStd, const VfsStatus &vfsStatus) {
    const SyncPath fullPath(_vfsSetupParams.localPath / pathStd);
    if (const ExitInfo exitInfo = checkIfPathIsValid(fullPath, true); !exitInfo) {
        return exitInfo;
    }
    // Update Finder
    LOGW_DEBUG(logger(), L"Send status to the Finder extension for file/directory " << Path2WStr(fullPath));
    if (_vfsSetupParams.executeCommand) {
        CommString command(Str("STATUS"));
        command.append(MESSAGE_CDE_SEPARATOR);
        command.append(CommonUtility::str2CommString(std::to_string(vfsStatus.isSyncing)));
        command.append(MESSAGE_ARG_SEPARATOR);
        command.append(CommonUtility::str2CommString(std::to_string(vfsStatus.progress)));
        command.append(MESSAGE_ARG_SEPARATOR);
        command.append(CommonUtility::str2CommString(std::to_string(vfsStatus.isHydrated)));
        command.append(MESSAGE_ARG_SEPARATOR);
        command.append(pathStd.native());
        _vfsSetupParams.executeCommand(command, true);
    }

    return ExitCode::Ok;
}

ExitInfo VfsOff::startImpl(bool &, bool &, bool &) {
    return ExitCode::Ok;
}

static QString modeToPluginName(const VirtualFileMode virtualFileMode) {
    if (virtualFileMode == VirtualFileMode::Win) return "win";
    if (virtualFileMode == VirtualFileMode::Mac) return "mac";
    return {};
}

bool KDC::isVfsPluginAvailable(const VirtualFileMode virtualFileMode, QString &error) {
    if (virtualFileMode == VirtualFileMode::Off) return true;

    if (virtualFileMode == VirtualFileMode::Suffix) {
        return false;
    }

    if (virtualFileMode == VirtualFileMode::Win) {
        if (CommonUtility::platform() == Platform::WindowsServer) return false; // LiteSync not available on Windows Server

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

VirtualFileMode KDC::bestAvailableVfsMode() {
    if (QString error; isVfsPluginAvailable(VirtualFileMode::Win, error)) {
        return VirtualFileMode::Win;
    } else if (isVfsPluginAvailable(VirtualFileMode::Mac, error)) {
        return VirtualFileMode::Mac;
    } else if (isVfsPluginAvailable(VirtualFileMode::Suffix, error)) {
        return VirtualFileMode::Suffix;
    }
    return VirtualFileMode::Off;
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
