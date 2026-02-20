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

#include "linuxupdater.h"

#include <sys/utsname.h>
#include <fstream>
#include <sstream>
#include <string>

namespace KDC {

static bool parseVersion(const std::string &versionStr, int &major, int &minor, int &patch) {
    major = minor = patch = 0;

    // Try format: major.minor.patch
    if (sscanf(versionStr.c_str(), "%d.%d.%d", &major, &minor, &patch) == 3) {
        return true;
    }

    // Try format: major.minor
    if (sscanf(versionStr.c_str(), "%d.%d", &major, &minor) == 2) {
        return true;
    }

    // Try format: major only
    if (sscanf(versionStr.c_str(), "%d", &major) == 1) {
        return true;
    }

    return false;
}

void LinuxUpdater::onUpdateFound() {
    setState(UpdateState::ManualUpdateAvailable);
}

bool LinuxUpdater::checkMinOsVersion(const std::string &minOsVersion) const {
    if (CommonUtility::distributionName() != "Ubuntu") return true; // Do not check OS version for distributions other than Ubuntu
    return AbstractUpdater::checkMinOsVersion(minOsVersion);
}

} // namespace KDC
