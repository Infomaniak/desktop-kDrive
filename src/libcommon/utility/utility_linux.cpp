/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#include "utility.h"

#include <QStandardPaths>

#include <filesystem>
#include <fstream>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#include "utility/types.h"

#include <log4cplus/logger.h>
#include <log4cplus/loggingmacros.h>

#include <Poco/File.h>
#include <Poco/Exception.h>

namespace KDC {

SyncPath CommonUtility::getGenericAppSupportDir() {
    const char *homeDir;
    if ((homeDir = getenv("HOME")) == NULL) {
        homeDir = getpwuid(getuid())->pw_dir;
    }
    SyncPath homePath(homeDir);

    std::string appSupportName(".config");
    SyncPath appSupportPath(homePath / appSupportName);

    std::error_code ec;
    if (!std::filesystem::exists(appSupportPath, ec)) {
        if (ec.value() != 0) {
            return {};
        }

        if (!std::filesystem::create_directory(appSupportPath, ec)) {
            return {};
        }
    }

    return appSupportPath;
}

SyncPath CommonUtility::getAppDir() {
    return {};
}

bool CommonUtility::hasDarkSystray() {
    return true;
}

SyncPath Utility::getTrashPath() {
    const char *homePathEnv = std::getenv("HOME");
    if (!homePathEnv) {
        LOG_WARN(Log::instance()->getLogger(), "Path to HOME not found");
        return {};
    }

    if (const char *xdgDataHomeEnv = std::getenv("XDG_DATA_HOME"); xdgDataHomeEnv) {
        return std::string(xdgDataHomeEnv) + "/Trash/files/";
    }

    return std::string(homePathEnv) + "/.local/share/Trash/files/";
}

} // namespace KDC
