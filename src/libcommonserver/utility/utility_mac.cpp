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

#include "log/log.h"
#include "libcommon/utility/utility.h"

#include <cstdlib>
#include <string>

#include <log4cplus/loggingmacros.h>

namespace KDC {

std::string Utility::userName() {
    bool isSet = false;
    return CommonUtility::envVarValue("USER", isSet);
}

SyncPath Utility::getTrashPath() {
    const char *homePathEnv = std::getenv("HOME");
    if (!homePathEnv) {
        LOG_WARN(logger(), "Path to HOME not found.");
        return {};
    }

    return SyncPath(homePathEnv) / ".Trash";
}

bool Utility::registerLoginRedirection() {
    // Working directory path is like "<path to the app bundle>/Contents/MacOS"
    const std::string appBundlePath = CommonUtility::getAppWorkingDir().parent_path().parent_path().native();
    (void) Utility::runCommand(
            "/System/Library/Frameworks/CoreServices.framework/Versions/A/Frameworks/LaunchServices.framework/Versions/A/Support/"
            "lsregister",
            {"-f", appBundlePath});
    return true;
}

} // namespace KDC
