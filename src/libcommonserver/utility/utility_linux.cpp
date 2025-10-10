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

#include "utility.h"

#include "log/log.h"
#include "libcommon/utility/utility.h"

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

bool Utility::moveItemToTrash(const SyncPath &itemPath) {
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
            LOG_WARN(Log::instance()->getLogger(),
                     "Error in std::filesystem::exists - err=" << ec.message() << " (" << std::to_string(ec.value()) << ")");
            return false;
        }

        if (!std::filesystem::create_directories(trash_path)) {
            LOG_WARN(Log::instance()->getLogger(), "Failed to create directory - path=" << trash_path.string());
            return false;
        }
    }

    int result = system(command.c_str());
    if (result != 0) {
        LOG_WARN(Log::instance()->getLogger(), "Failed to move item to trash - err=" << std::to_string(result));
        return false;
    }
    return true;
}

namespace {
int parseLineForRamStatus(char *line) {
    int i = static_cast<int>(strlen(line));
    const char *p = line;
    while (*p < '0' || *p > '9') p++;
    line[i - 3] = '\0';
    i = atoi(p);
    return i;
}

void initCpuUsage(uint64_t &lastTotalUser, uint64_t &lastTotalUserLow, uint64_t &lastTotalSys, uint64_t &lastTotalIdle) {
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

std::string executeCommand(const std::string &command) {
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
} // namespace

bool Utility::totalRamAvailable(uint64_t &ram, int &errorCode) {
    struct sysinfo memInfo;

    if (sysinfo(&memInfo) == 0) {
        ram = memInfo.totalram;
        ram *= memInfo.mem_unit;
        return true;
    }
    errorCode = errno;
    return false;
}

bool Utility::ramCurrentlyUsed(uint64_t &ram, int &errorCode) {
    struct sysinfo memInfo;

    if (sysinfo(&memInfo) == 0) {
        ram = memInfo.totalram - memInfo.freeram;
        ram *= memInfo.mem_unit;
        return true;
    }
    errorCode = errno;
    return false;
}

bool Utility::ramCurrentlyUsedByProcess(uint64_t &ram, int &errorCode) {
    FILE *file = fopen("/proc/self/status", "r");
    if (!file) {
        errorCode = errno;
        return false;
    }
    char line[128];

    while (fgets(line, 128, file) != nullptr) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            ram = static_cast<uint64_t>(parseLineForRamStatus(line));
            break;
        }
    }
    fclose(file);
    return true;
}

bool Utility::cpuUsage(uint64_t &lastTotalUser, uint64_t &lastTotalUserLow, uint64_t &lastTotalSys, uint64_t &lastTotalIdle,
                       double &percent) {
    if (lastTotalUser == 0 && lastTotalUser == 0 && lastTotalSys == 0 && lastTotalIdle == 0) {
        initCpuUsage(lastTotalUser, lastTotalUserLow, lastTotalSys, lastTotalIdle);
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
        percent = static_cast<double>(total);
        if (total > 0) {
            total += (totalIdle - lastTotalIdle);
            percent /= static_cast<double>(total);
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

bool Utility::cpuUsageByProcess(double &percent) {
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

std::string Utility::userName() {
    bool isSet = false;
    return CommonUtility::envVarValue("USER", isSet);
}

namespace {
// Returns the autostart directory the linux way
// and respects the XDG_CONFIG_HOME env variable
SyncPath getUserAutostartDir() {
    auto configPath = CommonUtility::getAppSupportDir().parent_path();
    configPath /= "autostart";

    return configPath;
}
} // namespace

bool Utility::hasLaunchOnStartup(const std::string &appName) {
    const auto desktopFileLocation = getUserAutostartDir() / (appName + ".desktop");
    return std::filesystem::exists(desktopFileLocation);
}

bool Utility::setLaunchOnStartup(const std::string &appName, const std::string &guiName, bool enable) {
    const auto userAutoStartDirPath = getUserAutostartDir();
    const auto userAutoStartFilePath = userAutoStartDirPath / (appName + ".desktop");
    if (enable) {
        IoError ioError = IoError::Unknown;
        if (!std::filesystem::exists(userAutoStartDirPath) && !IoHelper::createDirectory(userAutoStartDirPath, false, ioError)) {
            LOGW_WARN(logger(), L"Could not create autostart folder" << Utility::formatSyncPath(userAutoStartDirPath)
                                                                     << Utility::formatIoError(ioError));
            return false;
        }

        std::wofstream testFile(userAutoStartFilePath, std::ios_base::in);
        if (!testFile.is_open()) {
            LOGW_WARN(logger(), L"Could not write auto start entry." << Utility::formatSyncPath(userAutoStartFilePath));
            return false;
        }
        const auto appimageDir = CommonUtility::envVarValue("APPIMAGE");
        LOG_DEBUG(logger(), "APPIMAGE=" << appimageDir);
        testFile << L"[Desktop Entry]" << std::endl;
        testFile << L"Name=" << CommonUtility::s2ws(guiName) << std::endl;
        testFile << L"GenericName=File Synchronizer" << std::endl;
        testFile << L"Exec=" << Utility::formatSyncPath(appimageDir) << std::endl;
        testFile << L"Terminal=false" << std::endl;
        testFile << L"Icon=" << CommonUtility::s2ws(CommonUtility::toLower(appName)) << std::endl;
        testFile << L"Categories=Network" << std::endl;
        testFile << L"Type=Application" << std::endl;
        testFile << L"StartupNotify=false" << std::endl;
        testFile << L"X-GNOME-Autostart-enabled=true" << std::endl;
        testFile << L"X-GNOME-Autostart-Delay=10" << std::endl;
        testFile.close();
    } else {
        IoError ioError = IoError::Unknown;
        if (!IoHelper::deleteItem(userAutoStartFilePath, ioError)) {
            LOGW_WARN(logger(), L"Could not remove autostart desktop file." << Utility::formatIoError(ioError));
            return false;
        }
    }

    return true;
}

bool Utility::hasSystemLaunchOnStartup(const std::string &) {
    return false;
}

} // namespace KDC
