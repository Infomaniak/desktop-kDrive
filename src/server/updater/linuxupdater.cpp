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

#include "jobs/network/directdownloadjob.h"
#include "io/iohelper.h"
#include "libcommonserver/utility/utility.h"
#include "log/log.h"

#include <sys/utsname.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace KDC {

void LinuxUpdater::onUpdateFound() {
    setState(UpdateState::ManualUpdateAvailable);
}

bool LinuxUpdater::checkMinOsVersion(const std::string &minOsVersion) const {
    if (CommonUtility::distributionName() != "Ubuntu") return true; // Do not check OS version for distributions other than Ubuntu
    return AbstractUpdater::checkMinOsVersion(minOsVersion);
}

ExitCode LinuxUpdater::installVersion() {
    const auto &urlStr = versionInfo().downloadUrl;
    if (urlStr.empty()) {
        LOG_ERROR(Log::instance()->getLogger(), "Download URL is empty.");
        return ExitCode::SystemError;
    }

    const char *homeDir = std::getenv("HOME");
    if (!homeDir) {
        LOG_ERROR(Log::instance()->getLogger(), "HOME environment variable not set.");
        return ExitCode::SystemError;
    }

    const SyncPath destDir = std::filesystem::path(homeDir) / "Applications";
    std::filesystem::create_directories(destDir);

    const auto pos = urlStr.find_last_of('/');
    if (pos == std::string::npos) {
        LOG_ERROR(Log::instance()->getLogger(), "Invalid download URL.");
        return ExitCode::SystemError;
    }
    const auto filename = urlStr.substr(pos + 1);
    const SyncPath destPath = destDir / filename;

    // Remove an eventual already existing file.
    auto ioError = IoError::Success;
    (void) IoHelper::deleteItem(destPath, ioError);

    // Download synchronously
    const auto job = std::make_shared<DirectDownloadJob>(destPath, urlStr);
    if (!job->runSynchronously()) {
        if (job->httpResponse().getStatus() == 404) {
            LOGW_WARN(Log::instance()->getLogger(), L"Version not found (404).");
        } else {
            LOGW_WARN(Log::instance()->getLogger(), L"Download failed.");
        }
        return ExitCode::NetworkError;
    }

    if (std::error_code ec; !std::filesystem::exists(destPath, ec)) {
        LOGW_ERROR(Log::instance()->getLogger(), L"Downloaded file not found.");
        return ExitCode::SystemError;
    }

    // Make executable
    try {
        std::filesystem::permissions(
                destPath,
                std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec |
                        std::filesystem::perms::others_exec,
                std::filesystem::perm_options::add);
    } catch (const std::filesystem::filesystem_error &e) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Failed to make AppImage executable: " << CommonUtility::s2ws(e.what()));
    }

    return ExitCode::Ok;
}

} // namespace KDC
