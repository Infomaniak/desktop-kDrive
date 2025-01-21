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

#include <QStandardPaths>

#include <filesystem>
#include <fstream>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#include "libcommon/utility/types.h"

#include <log4cplus/logger.h>
#include <log4cplus/loggingmacros.h>

#include <Poco/File.h>
#include <Poco/Exception.h>

namespace KDC {

static SyncPath getAppSupportDir_private() {
    const char *homeDir;
    if ((homeDir = getenv("HOME")) == NULL) {
        homeDir = getpwuid(getuid())->pw_dir;
    }
    std::filesystem::path homePath(homeDir);

    std::string appSupportName(".config");
    std::filesystem::path appSupportPath(homePath / appSupportName);

    std::error_code ec;
    if (!std::filesystem::exists(appSupportPath, ec)) {
        if (ec.value() != 0) {
            return std::string();
        }

        if (!std::filesystem::create_directory(appSupportPath, ec)) {
            return std::string();
        }
    }

    return appSupportPath.string();
}

static std::string getAppDir_private() {
    return "";
}

static inline bool hasDarkSystray_private() {
    return true;
}

} // namespace KDC
