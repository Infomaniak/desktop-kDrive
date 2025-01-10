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

Vfs::Vfs(VfsSetupParams &vfsSetupParams, QObject *parent) :
    QObject(parent), _vfsSetupParams(vfsSetupParams), _extendedLog(false), _started(false) {}

Vfs::~Vfs() {}

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

bool Vfs::start(bool &installationDone, bool &activationDone, bool &connectionDone) {
    if (!_started) {
        _started = startImpl(installationDone, activationDone, connectionDone);
        return _started;
    }

    return true;
}

void Vfs::stop(bool unregister) {
    if (_started) {
        stopImpl(unregister);
        _started = false;
    }
}

VfsOff::VfsOff(VfsSetupParams &vfsSetupParams, QObject *parent) : Vfs(vfsSetupParams, parent) {}

VfsOff::~VfsOff() {}

bool VfsOff::forceStatus(const QString &path, bool isSyncing, int /*progress*/, bool /*isHydrated*/) {
    KDC::SyncPath fullPath(_vfsSetupParams._localPath / QStr2Path(path));
    bool exists = false;
    KDC::IoError ioError = KDC::IoError::Success;
    if (!KDC::IoHelper::checkIfPathExists(fullPath, exists, ioError)) {
        LOGW_WARN(logger(), L"Error in IoHelper::checkIfPathExists: " << KDC::Utility::formatIoError(fullPath, ioError).c_str());
        return false;
    }

    if (!exists) {
        LOGW_DEBUG(logger(), L"Item does not exist anymore - path=" << Path2WStr(fullPath).c_str());
        return true;
    }

    // Update Finder
    LOGW_DEBUG(logger(), L"Send status to the Finder extension for file/directory " << Path2WStr(fullPath).c_str());
    QString status = isSyncing ? "SYNC" : "OK";
    _vfsSetupParams._executeCommand(QString("STATUS:%1:%2").arg(status, path).toStdString().c_str());

    return true;
}

bool VfsOff::startImpl(bool &, bool &, bool &) {
    return true;
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
