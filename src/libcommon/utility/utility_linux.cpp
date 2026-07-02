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

#include <filesystem>
#include <fstream>
#include <vector>
#include <unistd.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <pwd.h>
#include <libmount/libmount.h>

#include "utility/types.h"

#include <config.h>

namespace KDC {

static std::string homeDirectoryStr() {
    if (auto homeDir = CommonUtility::envVarValue("HOME"); !homeDir.empty()) return homeDir;

    // The "HOME" environment variable might not be set. In this case, fallback on a more robust method by using getpwuid_r. The
    // "passwd" struct retrieved contains information such as username, user id, encrypted password or  home directory.
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
    const auto homeDir = homeDirectoryStr();
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

bool CommonUtility::fileSystemInfo(const SyncPath &targetPath, std::string &fsType, SyncPath &mountPoint) {
    fsType.clear();
    mountPoint.clear();

    // FS type
    struct statfs stat;
    if (statfs(targetPath.native().c_str(), &stat) != 0) return false;

    switch (stat.f_type) {
        case 0x4d44u: // MSDOS_SUPER_MAGIC
            fsType = "MSDOS";
            break;
        case 0x2011bab0u: // EXFAT_SUPER_MAGIC
            fsType = "EXFAT";
            break;
        case 0x137du: // EXT_SUPER_MAGIC
            fsType = "EXT";
            break;
        case 0xef51u: // EXT2_OLD_SUPER_MAGIC
            fsType = "EXT2";
            break;
        case 0xef53u: // EXT2_SUPER_MAGIC, EXT3_SUPER_MAGIC, EXT4_SUPER_MAGIC
            fsType = "EXT234";
            break;
        case 0xa501fcf5u: // VXFS_SUPER_MAGIC
            fsType = "VXFS";
            break;
        case 0x58465342u: // XFS_SUPER_MAGIC
            fsType = "XFS";
            break;
        case 0x9123683eu: // BTRFS_SUPER_MAGIC
            fsType = "BTRFS";
            break;
        case 0x73727279u: // BTRFS_TEST_MAGIC
            fsType = "BTRFS_TEST";
            break;
        case 0xf15fu: // ECRYPTFS_SUPER_MAGIC
            fsType = "ECRYPTFS";
            break;
        case 0x4244u: // HFS_SUPER_MAGIC
            fsType = "HFS";
            break;
        case 0x5346544eu: // NTFS_SB_MAGIC
            fsType = "NTFS";
            break;
        case 0x858458f6u: // RAMFS_MAGIC
            fsType = "RAMFS";
            break;
        case 0x0x65735546: // FUSE_SUPER_MAGIC
            fsType = "FUSE";
            break;
        default:
            // See corresponding entry at https://man7.org/linux/man-pages/man2/statfs.2.html
            fsType = std::to_string(stat.f_type);
    }

    // Mount point
    // NB: If no matching mount point found, fallback to the root directory
    libmnt_cache *cache = mnt_new_cache();
    libmnt_table *table = mnt_new_table();
    mnt_table_set_cache(table, cache);
    if (mnt_table_parse_file(table, "/proc/self/mountinfo") == 0) {
        if (libmnt_fs *fs = mnt_table_find_target(table, targetPath.native().c_str(), MNT_ITER_FORWARD); fs) {
            std::string mp = mnt_fs_get_target(fs);
            mountPoint = SyncPath(mp);
        }
    }
    mnt_free_table(table);
    mnt_free_cache(cache);

    return true;
}

ExitInfo CommonUtility::logDirectoryPath(SyncPath &directoryPath) noexcept {
    // XDG Base Directory Specification: use $XDG_STATE_HOME if it is absolute,
    // otherwise fallback to ~/.local/state
    const auto xdgStateHome = envVarValue("XDG_STATE_HOME");
    const auto xdgStateHomePath = SyncPath(xdgStateHome);
    if (!xdgStateHome.empty() && xdgStateHomePath.is_absolute()) {
        directoryPath = xdgStateHomePath;
    } else {
        const auto homeDir = homeDirectoryStr();
        if (homeDir.empty()) return {ExitCode::SystemError, ExitCause::NotFound};
        directoryPath = SyncPath(homeDir) / ".local" / "state";
    }

    directoryPath /= Str2SyncName(APPLICATION_NAME);
    directoryPath /= "logs";

    std::error_code ec;
    const bool directoryPathExists = std::filesystem::exists(directoryPath, ec);
    if (ec) {
        return stdErrorToExitInfo(ec);
    }

    if (directoryPathExists) {
        if (!std::filesystem::is_directory(directoryPath, ec)) {
            if (ec) {
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
