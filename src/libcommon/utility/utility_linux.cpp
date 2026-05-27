// Infomaniak kDrive - Desktop
// Copyright (C) 2023-2026 Infomaniak Network SA
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "utility.h"

#include <filesystem>
#include <fstream>
#include <vector>
#include <unistd.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <pwd.h>

#include "utility/types.h"

#include <config.h>

namespace KDC {

static std::string getHomeDirFromPasswd() {
    struct passwd pwd;
    struct passwd *result = nullptr;
    auto bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (bufsize == -1) bufsize = 16384;
    if (std::vector<char> buf(static_cast<size_t>(bufsize));
        getpwuid_r(getuid(), &pwd, buf.data(), buf.size(), &result) == 0 && result != nullptr) {
        return std::string(result->pw_dir);
    }
    return {};
}

SyncPath CommonUtility::getGenericAppSupportDir() {
    auto homeDir = CommonUtility::envVarValue("HOME");
    if (homeDir.empty()) homeDir = getHomeDirFromPasswd();
    if (homeDir.empty()) return {};

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

/**
 * @brief On Linux, version information are extracted from the file /etc/os-release
 * Example of content:
 *  PRETTY_NAME="Ubuntu 24.04.3 LTS"
 *  NAME="Ubuntu"
 *  VERSION_ID="24.04"
 *  VERSION="24.04.3 LTS (Noble Numbat)"
 *  VERSION_CODENAME=noble
 *  ID=ubuntu
 *  ID_LIKE=debian
 *  HOME_URL="https://www.ubuntu.com/"
 *  SUPPORT_URL="https://help.ubuntu.com/"
 *  BUG_REPORT_URL="https://bugs.launchpad.net/ubuntu/"
 *  PRIVACY_POLICY_URL="https://www.ubuntu.com/legal/terms-and-policies/privacy-policy"
 *  UBUNTU_CODENAME=noble
 *  LOGO=ubuntu-logo
 * @return The value associated with the key
 */
std::string extractOsInfo(const std::string &key) {
    std::string value;
    // Try to get version from /etc/os-release (standard on modern Linux distributions)
    if (std::ifstream osRelease("/etc/os-release"); osRelease.is_open()) {
        std::string line;
        while (std::getline(osRelease, line)) {
            if (line.find(key) != 0) continue;

            value = line.substr(key.length() + 1); // +1 because the file is formatted "key=value"
            // Remove quotes if present
            if (!value.empty() && value.front() == '"') {
                value = value.substr(1);
            }
            if (!value.empty() && value.back() == '"') {
                value.pop_back();
            }
            break;
        }
    }
    return value;
}

std::string CommonUtility::osVersion() {
    static const std::string osVersion = extractOsInfo("VERSION_ID");
    return osVersion;
}

std::string CommonUtility::distributionName() {
    static const std::string distributionName = extractOsInfo("NAME");
    return distributionName;
}

namespace {
#ifndef EXFAT_SUPER_MAGIC
#define EXFAT_SUPER_MAGIC 0x2011BAB0
#endif

constexpr auto exFat = "exFAT";
constexpr auto ext234 = "EXT2/3/4";

std::string formatFsName(const std::string &prettyName, const __fsword_t fType) {
    std::stringstream stream;
    stream << std::hex << fType;
    return prettyName + " | 0x" + stream.str();
}
} // namespace

bool CommonUtility::isEXT234(const SyncPath &targetPath) {
    return contains(getRootFsType(targetPath), ext234);
}

std::string CommonUtility::exFAT() {
    return formatFsName(exFat, EXFAT_SUPER_MAGIC);
}

std::string CommonUtility::fileSystemName(const SyncPath &targetPath) {
    struct statfs stat;

    if (statfs(targetPath.root_path().native().c_str(), &stat) == 0) {
        switch (stat.f_type) {
            case EXFAT_SUPER_MAGIC:
                return exFAT();
            case 0x137du:
                return formatFsName("EXT(1)", stat.f_type);
            case 0xef51u:
                return formatFsName("EXT2", stat.f_type);
            case 0xef53u: // EXT_SUPER_MAGIC
                return formatFsName(ext234, stat.f_type);
            case 0xbad1deau:
            case 0xa501fcf5u:
            case 0x58465342u:
                return formatFsName("XFS", stat.f_type);
            case 0x9123683eu:
            case 0x73727279u:
                return formatFsName("BTRFS", stat.f_type);
            case 0xf15fu:
                return formatFsName("ECRYPTFS", stat.f_type);
            case 0x4244u:
                return formatFsName("HFS", stat.f_type);
            case 0x5346544eu:
                return formatFsName("NTFS", stat.f_type);
            case 0x858458f6u:
                return formatFsName("RAMFS", stat.f_type);
            default:
                return formatFsName("Unknown-see corresponding entry at https://man7.org/linux/man-pages/man2/statfs.2.html",
                                    stat.f_type);
        }
    }

    return "UNIDENTIFIED";
}

ExitInfo CommonUtility::logDirectoryPath(SyncPath &directoryPath) noexcept {
    // XDG Base Directory Specification: use $XDG_STATE_HOME if it is absolute,
    // otherwise fallback to ~/.local/state
    const auto xdgStateHome = envVarValue("XDG_STATE_HOME");
    const auto xdgStateHomePath = SyncPath(xdgStateHome);
    if (!xdgStateHome.empty() && xdgStateHomePath.is_absolute()) {
        directoryPath = xdgStateHomePath;
    } else {
        auto homeDir = envVarValue("HOME");
        if (homeDir.empty()) homeDir = getHomeDirFromPasswd();
        if (homeDir.empty()) return {ExitCode::SystemError, ExitCause::NotFound};
        directoryPath = SyncPath(homeDir) / ".local" / "state";
    }

    directoryPath /= Str2SyncName(APPLICATION_NAME);
    directoryPath /= "logs";

    std::error_code ec;
    const bool directoryPathExists = std::filesystem::exists(directoryPath, ec);
    if (ec.value() != 0) {
        return stdErrorToExitInfo(ec);
    }

    if (directoryPathExists) {
        if (!std::filesystem::is_directory(directoryPath, ec)) {
            if (ec.value() != 0) {
                return stdErrorToExitInfo(ec);
            }
            return stdErrorToExitInfo(std::make_error_code(std::errc::not_a_directory));
        }
    } else if (!std::filesystem::create_directories(directoryPath, ec)) {
        return stdErrorToExitInfo(ec);
    }

    return ExitCode::Ok;
}

} // namespace KDC
