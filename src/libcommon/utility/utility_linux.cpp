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

std::string CommonUtility::osVersion() {
    std::string versionId;
    // Try to get version from /etc/os-release (standard on modern Linux distributions)
    std::ifstream osRelease("/etc/os-release");
    if (osRelease.is_open()) {
        std::string line;
        while (std::getline(osRelease, line)) {
            if (line.find("VERSION_ID=") == 0) {
                versionId = line.substr(11);
                // Remove quotes if present
                if (!versionId.empty() && versionId.front() == '"') {
                    versionId = versionId.substr(1);
                }
                if (!versionId.empty() && versionId.back() == '"') {
                    versionId.pop_back();
                }
                break;
            }
        }
    }
    return versionId;
}

} // namespace KDC
