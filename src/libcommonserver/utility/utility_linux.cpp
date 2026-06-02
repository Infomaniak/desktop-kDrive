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
#include "io/iohelper.h"

#include "log/log.h"
#include "libcommon/utility/utility.h"

#include <config.h>
#include <filesystem>
#include <regex>
#include <string>
#include <pwd.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <sys/wait.h>
#include <unistd.h>

#include <log4cplus/loggingmacros.h>

#include <Poco/Timestamp.h>
#include <Poco/File.h>
#include <Poco/Exception.h>

namespace KDC {

static const auto mimeType = "x-scheme-handler/kdrive";

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
            LOGW_WARN(logger(), L"Could not create autostart folder: " << Utility::formatIoError(userAutoStartDirPath, ioError));
            return false;
        }

        std::ofstream autoStartFile{userAutoStartFilePath};
        if (!autoStartFile.is_open()) {
            LOGW_WARN(logger(), L"Could not create autostart desktop file: " << Utility::formatSyncPath(userAutoStartFilePath));
            return false;
        }
        const SyncPath appimageDir{CommonUtility::envVarValue("APPIMAGE")};
        LOGW_DEBUG(logger(), L"APPIMAGE: " << Utility::formatSyncPath(appimageDir));
        autoStartFile << "[Desktop Entry]" << std::endl;
        autoStartFile << "Name=" << guiName << std::endl;
        autoStartFile << "GenericName=File Synchronizer" << std::endl;
        autoStartFile << "Exec=" << "'" << appimageDir.native() << "'" << std::endl;
        autoStartFile << "Terminal=false" << std::endl;
        autoStartFile << "Icon=" << CommonUtility::toLower(appName) << std::endl;
        autoStartFile << "Categories=Network" << std::endl;
        autoStartFile << "Type=Application" << std::endl;
        autoStartFile << "StartupNotify=false" << std::endl;
        autoStartFile << "X-GNOME-Autostart-enabled=true" << std::endl;
        autoStartFile << "X-GNOME-Autostart-Delay=10" << std::endl;
        autoStartFile.close();
    } else {
        IoError ioError = IoError::Unknown;
        if (!IoHelper::deleteItem(userAutoStartFilePath, ioError)) {
            LOGW_WARN(logger(),
                      L"Could not remove autostart desktop file: " << Utility::formatIoError(userAutoStartDirPath, ioError));
            return false;
        }
    }

    return true;
}

bool Utility::hasSystemLaunchOnStartup(const std::string &) {
    return false;
}

SyncPath Utility::getTrashPath() {
    const char *homePathEnv = std::getenv("HOME");
    if (!homePathEnv) {
        LOG_WARN(Log::instance()->getLogger(), "Path to HOME not found");
        return {};
    }

    if (const char *xdgDataHomeEnv = std::getenv("XDG_DATA_HOME"); xdgDataHomeEnv) {
        return std::string(xdgDataHomeEnv) + "/Trash/files/";
    }

    return std::string(homePathEnv) + "/.local/share/Trash/files/";
}

bool Utility::registerLoginRedirection() {
    // Create file .desktop
    const char *homePathEnv = std::getenv("HOME");
    if (!homePathEnv) {
        LOG_WARN(Log::instance()->getLogger(), "Path to HOME not found");
        return false;
    }

    const auto urlSchemeDirPath = SyncPath(std::string(homePathEnv) + "/.local/share/applications");
    const auto urlSchemeFilePath = urlSchemeDirPath / (std::string(APPLICATION_EXECUTABLE) + ".desktop");

    IoError ioError = IoError::Unknown;
    bool exists = false;
    if (!IoHelper::checkIfPathExists(urlSchemeDirPath, exists, ioError, IoHelper::PathCheckOption::Insensitive)) {
        LOGW_WARN(logger(), L"Error in IoHelper::checkIfPathExists:" << Utility::formatIoError(urlSchemeDirPath, ioError));
        return false;
    }
    if (!exists && !IoHelper::createDirectory(urlSchemeDirPath, false, ioError)) {
        LOGW_WARN(logger(), L"Could register login redirection: " << Utility::formatIoError(urlSchemeDirPath, ioError));
        return false;
    }

    std::ofstream urlSchemeFile{urlSchemeFilePath};
    if (!urlSchemeFile.is_open()) {
        LOGW_WARN(logger(), L"Could register login redirection: " << Utility::formatSyncPath(urlSchemeFilePath));
        return false;
    }

    SyncPath execPath;
    const std::string appImageEnv = KDC::CommonUtility::envVarValue("APPIMAGE");
    if (!appImageEnv.empty()) {
        execPath = SyncPath(appImageEnv);
    } else {
        execPath = KDC::CommonUtility::getAppWorkingDir() / APPLICATION_EXECUTABLE;
    }
    urlSchemeFile << "[Desktop Entry]" << std::endl;
    urlSchemeFile << "Name=" << APPLICATION_EXECUTABLE << std::endl;
    urlSchemeFile << "Exec=" << "\"" << execPath.string() << "\"" << " %u" << std::endl;
    urlSchemeFile << "Type=Application" << std::endl;
    urlSchemeFile << "Terminal=false" << std::endl;
    urlSchemeFile << "MimeType=" << mimeType << ";" << std::endl;
    urlSchemeFile.close();

    // Update database
    bool res = true;
    const std::string applicationsDir = std::string(homePathEnv) + "/.local/share/applications/";
    if (!Utility::runCommand("update-desktop-database", {applicationsDir})) {
        LOGW_WARN(logger(), L"Failed to update desktop database for: " << CommonUtility::s2ws(applicationsDir));
        res = false; // Do not return yet, try to register scheme anyway.
    }

    // Register scheme
    if (!Utility::runCommand("xdg-mime", std::vector<std::string>{"default", "kDrive.desktop", mimeType})) {
        LOGW_WARN(logger(), L"Failed to register URL scheme with xdg-mime");
        res = false;
    }

    return res;
}

ExitInfo Utility::getFileSystemName(const std::shared_ptr<CacheDirectory> cacheDirectory, std::string &fileSystemName) {
    fileSystemName = {};

    if (!cacheDirectory) {
        LOG_WARN(logger(), "Cache directory not provided!");
        return {ExitCode::SystemError, ExitCause::InvalidArgument};
    }

    SyncPath localSyncPath;
    if (const auto exitInfo = cacheDirectory->path(localSyncPath); !exitInfo) return exitInfo;

    static std::unordered_map<SyncPath, std::string> cacheDirectoryToFileSystemName;

    const auto it = cacheDirectoryToFileSystemName.find(localSyncPath);
    if (it != cacheDirectoryToFileSystemName.end()) {
        fileSystemName = it->second;
        return ExitCode::Ok;
    }

    fileSystemName = CommonUtility::fileSystemName(localSyncPath);
    cacheDirectoryToFileSystemName[localSyncPath] = fileSystemName;

    if (!CommonUtility::isEXT234(localSyncPath)) return ExitCode::Ok;

    // `statfs` can confuse more restrictive systems with `EXT2/3/4 when a USB stick is used.
    constexpr auto invalidExFatFileName = "a:b";

    const auto exitInfo = tryCreateTmpFile(cacheDirectory, invalidExFatFileName);
    if (exitInfo.cause() == ExitCause::TmpDirAccessError) {
        LOG_WARN(logger(), "Cannot access tmp directory.");

        return exitInfo;
    }

    if (exitInfo.cause() == ExitCause::InvalidName) {
        LOG_DEBUG(logger(),
                  "File names containing a colon character are not valid for the filesystem in use. We shall assume that "
                  "this file system is exFAT.");
        fileSystemName = CommonUtility::exFAT();
        cacheDirectoryToFileSystemName[localSyncPath] = fileSystemName;

        return ExitCode::Ok;
    }

    return exitInfo;
}

bool Utility::runCommand(const std::string &launchPath, const std::vector<std::string> &arguments) {
    // Build argv array
    std::vector<char *> argv;
    argv.push_back(const_cast<char *>(launchPath.c_str()));
    for (const auto &arg: arguments) {
        argv.push_back(const_cast<char *>(arg.c_str()));
    }
    argv.push_back(nullptr);

    pid_t pid = 0;
    int spawnStatus = posix_spawnp(&pid, launchPath.c_str(), nullptr, nullptr, argv.data(), environ);
    if (spawnStatus != 0) {
        LOG_ERROR(logger(), "Failed to spawn process " << launchPath << ": " << strerror(spawnStatus));
        return false;
    }

    int waitStatus = 0;
    if (waitpid(pid, &waitStatus, 0) == -1) {
        LOG_ERROR(logger(), "Failed to wait for process " << launchPath << ": " << strerror(errno));
        return false;
    }

    if (WIFEXITED(waitStatus)) {
        const int exitStatus = WEXITSTATUS(waitStatus);
        if (exitStatus != 0) {
            LOG_WARN(logger(), "Command " << launchPath << " exited with status " << exitStatus);
        }
        return exitStatus == 0;
    }

    LOG_WARN(logger(), "Command " << launchPath << " terminated abnormally");
    return false;
}


} // namespace KDC
