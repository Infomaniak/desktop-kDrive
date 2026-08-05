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

#include "runningprocessinfo_linux.h"

#include <charconv>
#include <fstream>
#include <system_error>

#include <sys/stat.h>
#include <unistd.h>

namespace KDC {
namespace {

std::optional<uint32_t> processOwnerId(const std::filesystem::path &processPath) {
    struct stat processStatus{};
    if (stat(processPath.c_str(), &processStatus) != 0) {
        return std::nullopt;
    }
    return processStatus.st_uid;
}

} // namespace

std::optional<int64_t> runningProcessPid(const std::string &processName) {
    return runningProcessPid(processName, "/proc", geteuid(), getpid());
}

/**
 * Finds another process with the expected name which is owned by the current user.
 * All filesystem operations which can race with process termination are non-throwing so a vanished process only skips its entry.
 */
std::optional<int64_t> runningProcessPid(const std::string &processName, const std::filesystem::path &procRoot,
                                         const uint32_t effectiveUserId, const int64_t currentPid) {
    std::error_code errorCode;
    std::filesystem::directory_iterator processEntry(procRoot, errorCode);
    if (errorCode) {
        return std::nullopt;
    }

    // errors on directory_iterator#increment can occur only if there is a problem on /proc itself (unmounted procfs, ...)
    std::error_code ignoredErrorCode;
    for (const std::filesystem::directory_iterator end; processEntry != end; (void) processEntry.increment(ignoredErrorCode)) {
        // the entry should be a directory
        if (std::error_code entryErrorCode; !processEntry->is_directory(entryErrorCode) || entryErrorCode) {
            continue;
        }

        // parse the directory name as an int64
        const auto pidString = processEntry->path().filename().string();
        int64_t pid = 0;
        if (const auto [endPtr, parseError] = std::from_chars(pidString.data(), pidString.data() + pidString.size(), pid);
            parseError != std::errc{} || endPtr != pidString.data() + pidString.size() || pid == currentPid) {
            continue;
        }

        // the owner of the process should be the same as us
        if (const auto ownerId = processOwnerId(processEntry->path()); !ownerId.has_value() || *ownerId != effectiveUserId) {
            continue;
        }

        // read the name of the process
        std::ifstream commFile(processEntry->path() / "comm");
        std::string currentProcessName;
        if (!commFile.is_open() || !(commFile >> currentProcessName) || commFile.bad()) {
            continue;
        }

        // the name of the process should be the same as us
        if (currentProcessName != processName) {
            continue;
        }

        return pid;
    }
    return std::nullopt;
}

} // namespace KDC
