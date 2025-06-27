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

#pragma once

#include "libcommon/info/nodeinfo.h"

#include <functional>
#include <memory>

#include <QString>
#include <QDateTime>

#include <log4cplus/logger.h>

#ifdef Q_OS_WIN
#define UMDF_USING_NTSTATUS
#include <windows.h>
#endif

class QSettings;

namespace KDC {

namespace OldUtility {

#ifdef _WIN32
void setFolderPinState(const QUuid &clsid, bool show);
#endif

#ifdef Q_OS_WIN
bool registryExistKeyTree(HKEY hRootKey, const QString &subKey);
bool registryExistKeyValue(HKEY hRootKey, const QString &subKey, const QString &valueName);
QVariant registryGetKeyValue(HKEY hRootKey, const QString &subKey, const QString &valueName);
bool registrySetKeyValue(HKEY hRootKey, const QString &subKey, const QString &valueName, DWORD type, const QVariant &value,
                         QString &error);
bool registryDeleteKeyTree(HKEY hRootKey, const QString &subKey);
bool registryDeleteKeyValue(HKEY hRootKey, const QString &subKey, const QString &valueName);
bool registryWalkSubKeys(HKEY hRootKey, const QString &subKey, const std::function<void(HKEY, const QString &)> &callback);

// Add/remove legacy sync root keys
void addLegacySyncRootKeys(const QUuid &clsid, const QString &folderPath, bool show);
void removeLegacySyncRootKeys(const QUuid &clsid);

// Possibly refactor to share code with UnixTimevalToFileTime in c_time.c
void UnixTimeToFiletime(time_t t, FILETIME *filetime);
#endif

} // namespace OldUtility


} // namespace KDC
