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

#include "libcommon/utility/types.h"

#include <QCoreApplication>
#include <QDir>
#include <QOperatingSystemVersion>
#include <QString>
#include <QLoggingCategory>

#include <CoreServices/CoreServices.h>
#include <CoreFoundation/CoreFoundation.h>

#include <log4cplus/logger.h>
#include <log4cplus/loggingmacros.h>

namespace KDC {

bool hasDarkSystray();
std::string getAppSupportDir();
std::string getAppDir();
bool setFolderCustomIcon(const QString &folderPath, const QString &icon);

} // namespace KDC

namespace KDC {

static bool hasDarkSystray_private() {
    return KDC::hasDarkSystray();
}

static KDC::SyncPath getAppSupportDir_private() {
    return KDC::getAppSupportDir();
}

static KDC::SyncPath getAppDir_private() {
    return KDC::getAppDir();
}

static bool setFolderCustomIcon_private(const QString &folderPath, const QString &iconPath) {
    if (iconPath.isEmpty()) {
        return true;
    }
    return KDC::setFolderCustomIcon(folderPath, iconPath);
}

} // namespace KDC
