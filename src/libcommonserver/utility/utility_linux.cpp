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

#include "log/log.h"

#include <filesystem>
#include <string>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <unistd.h>

#include <log4cplus/loggingmacros.h>

#include <Poco/Timestamp.h>
#include <Poco/File.h>
#include <Poco/Exception.h>

namespace KDC {

static bool init_private() {
    return true;
}

static void free_private() {}

static int moveItemToTrash_private(const SyncPath &itemPath) {
    std::string xdgDataHome, homePath, trashPath;

    const char *xdgDataHomeEnv = std::getenv("XDG_DATA_HOME");
    const char *homePathEnv = std::getenv("HOME");

    if (xdgDataHomeEnv) {
        xdgDataHome = std::string(xdgDataHomeEnv);
    }
    if (!homePathEnv) {
        LOG_WARN(Log::instance()->getLogger(), "Path to HOME not found");
        return false;
    }

    homePath = std::string(homePathEnv);

    if (xdgDataHome.empty()) {
        trashPath = homePath + "/.local/share/Trash/files/";
    } else {
        trashPath = xdgDataHome + "/Trash/files/";
    }

    std::string desktopType;
    if (!Utility::getLinuxDesktopType(desktopType)) {
        desktopType = "GNOME";
    }

    std::string command;
    if (desktopType == "GNOME") {
        command = "gio trash \"" + itemPath.string() + "\"";
    } else if (desktopType == "KDE") {
        command = "kioclient move \"" + itemPath.string() + "\" trash:/files/";
    } else {
        // Not implemented for the others distros
        return true;
    }

    std::filesystem::path trash_path(trashPath);

    // Check if the trash/files & trash/info path exists and create it if needed
    std::error_code ec;
    if (!std::filesystem::exists(trash_path, ec)) {
        if (ec.value() != 0) {
            LOG_WARN(Log::instance()->getLogger(), "Error in std::filesystem::exists - err=" << ec.message().c_str() << " ("
                                                                                             << std::to_string(ec.value()).c_str()
                                                                                             << ")");
            return false;
        }

        if (!std::filesystem::create_directories(trash_path)) {
            LOG_WARN(Log::instance()->getLogger(), "Failed to create directory - path=" << trash_path.string().c_str());
            return false;
        }
    }

    int result = system(command.c_str());
    if (result != 0) {
        LOG_WARN(Log::instance()->getLogger(), "Failed to move item to bin - err=" << std::to_string(result).c_str());
        return false;
    }
    return true;
}

int parseLineForRamStatus(char *line) {
    int i = strlen(line);
    const char *p = line;
    while (*p < '0' || *p > '9') p++;
    line[i - 3] = '\0';
    i = atoi(p);
    return i;
}

/* ---- RAM */
static bool totalRamAvailable_private(uint64_t &ram, int &errorCode) {
    struct sysinfo memInfo;

    if (sysinfo(&memInfo) == 0) {
        ram = memInfo.totalram;
        ram *= memInfo.mem_unit;
        return true;
    }
    errorCode = errno;
    return false;
}

static bool ramCurrentlyUsed_private(uint64_t &ram, int &errorCode) {
    struct sysinfo memInfo;

    if (sysinfo(&memInfo) == 0) {
        ram = memInfo.totalram - memInfo.freeram;
        ram *= memInfo.mem_unit;
        return true;
    }
    errorCode = errno;
    return false;
}

static bool ramCurrentlyUsedByProcess_private(uint64_t &ram, int &errorCode) {
    FILE *file = fopen("/proc/self/status", "r");
    if (!file) {
        errorCode = errno;
        return false;
    }
    char line[128];

    while (fgets(line, 128, file) != nullptr) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            ram = parseLineForRamStatus(line);
            break;
        }
    }
    fclose(file);
    return true;
}

static void initCpuUsage_private(uint64_t &lastTotalUser, uint64_t &lastTotalUserLow, uint64_t &lastTotalSys,
                                 uint64_t &lastTotalIdle) {
    FILE *file = fopen("/proc/stat", "r");
    if (!file) {
        return;
    }
    if (fscanf(file, "cpu %lu %lu %lu %lu", &lastTotalUser, &lastTotalUserLow, &lastTotalSys, &lastTotalIdle) != 4) {
        fclose(file);
        return;
    }
    fclose(file);
}

static bool cpuUsage_private(uint64_t &lastTotalUser, uint64_t &lastTotalUserLow, uint64_t &lastTotalSys, uint64_t &lastTotalIdle,
                             double &percent) {
    if (lastTotalUser == 0 && lastTotalUser == 0 && lastTotalSys == 0 && lastTotalIdle == 0) {
        initCpuUsage_private(lastTotalUser, lastTotalUserLow, lastTotalSys, lastTotalIdle);
    }

    FILE *file;
    uint64_t totalUser = 0;
    uint64_t totalUserLow;
    uint64_t totalSys;
    uint64_t totalIdle;
    uint64_t total;

    file = fopen("/proc/stat", "r");
    if (!file) {
        return false;
    }
    int ret = fscanf(file, "cpu %lu %lu %lu %lu", &totalUser, &totalUserLow, &totalSys, &totalIdle);
    fclose(file);

    if (ret != 4) {
        return false;
    }

    if (totalUser < lastTotalUser || totalUserLow < lastTotalUserLow || totalSys < lastTotalSys || totalIdle < lastTotalIdle) {
        // Overflow detection. Just skip this value.
        percent = -1.0;
        return false;
    } else {
        total = (totalUser - lastTotalUser) + (totalUserLow - lastTotalUserLow) + (totalSys - lastTotalSys);
        percent = total;
        if (total > 0) {
            total += (totalIdle - lastTotalIdle);
            percent /= total;
            percent *= 100;
        } else {
            percent = 0;
        }
    }

    lastTotalUser = totalUser;
    lastTotalUserLow = totalUserLow;
    lastTotalSys = totalSys;
    lastTotalIdle = totalIdle;

    return true;
}

static std::string executeCommand(const std::string &command) {
    std::array<char, 128> buffer{};
    std::string result;
    FILE *pipe = popen(command.c_str(), "r");
    if (!pipe) {
        throw std::runtime_error("Erreur lors de l'exécution de la commande");
    }
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    return result;
}

static bool cpuUsageByProcess_private(double &percent) {
    pid_t pid = getpid();
    const std::string getCPUCommand = "top -b -n 1 -p " + std::to_string(pid) + " | tail -1 | awk '{print $9 }'";
    std::string cpuUsageStr = executeCommand(getCPUCommand);

    double cpuUsage;
    std::istringstream iss(cpuUsageStr);
    if (!(iss >> cpuUsage)) {
        return false;
    }
    percent = cpuUsage;
    return true;
}

static bool setFileDates_private(const KDC::SyncPath &filePath, std::optional<KDC::SyncTime> creationDate,
                                 std::optional<KDC::SyncTime> modificationDate, bool &exists) {
    (void)creationDate;

    exists = true;

    try {
        Poco::Timestamp lastModifiedTimestamp(Poco::Timestamp::fromEpochTime(modificationDate.value()));
        Poco::File(Path2Str(filePath)).setLastModified(lastModifiedTimestamp);
    } catch (Poco::Exception &) {
        return false;
    }

    return true;
}

}  // namespace KDC
