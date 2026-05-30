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
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <regex>
#include <string>
#include <system_error>
#include <vector>
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

static const auto mimeType = "x-scheme-handler/kdrive";

namespace {
// Escape a string to be used as a quoted argument in a .desktop `Exec=` field
// (freedesktop.org Desktop Entry Specification §"The Exec key"). Reserved
// characters inside double quotes (`"`, `` ` ``, `$`, `\\`) must be prefixed
// with an additional backslash.
std::string escapeDesktopExecArg(const std::string &arg) {
    std::string out;
    out.reserve(arg.size() + 2);
    out.push_back('"');
    for (const char c: arg) {
        if (c == '"' || c == '\\' || c == '`' || c == '$') {
            out.push_back('\\');
        }
        out.push_back(c);
    }
    out.push_back('"');
    return out;
}

// POSIX single-quote escape for safe interpolation into a /bin/sh command line
// passed to `system()`. Single quotes are closed, the literal quote inserted
// via `'\''`, and the surrounding quoting re-opened.
std::string shellQuote(const std::string &arg) {
    std::string out;
    out.reserve(arg.size() + 2);
    out.push_back('\'');
    for (const char c: arg) {
        if (c == '\'') {
            out.append("'\\''");
        } else {
            out.push_back(c);
        }
    }
    out.push_back('\'');
    return out;
}

// Direct write to ~/.config/mimeapps.list, replicating what
// `xdg-mime default <desktop> <mime>` does, so the association is still set
// when xdg-utils is missing or returns a non-zero status (which happens on
// some minimal/Wayland-only setups).
bool writeMimeAppsDefault(const SyncPath &mimeAppsPath, const std::string &desktopFileName, const std::string &mimeTypeStr) {
    std::vector<std::string> lines;
    if (std::ifstream in{mimeAppsPath}; in.is_open()) {
        std::string line;
        while (std::getline(in, line)) lines.push_back(line);
    }

    const std::string sectionHeader = "[Default Applications]";
    const std::string keyPrefix = mimeTypeStr + "=";
    const std::string assocLine = keyPrefix + desktopFileName;

    auto sectionIt = std::find(lines.begin(), lines.end(), sectionHeader);
    if (sectionIt == lines.end()) {
        if (!lines.empty() && !lines.back().empty()) lines.emplace_back();
        lines.push_back(sectionHeader);
        lines.push_back(assocLine);
    } else {
        auto sectionEnd = std::find_if(std::next(sectionIt), lines.end(),
                                       [](const std::string &l) { return !l.empty() && l.front() == '['; });
        const auto existing =
                std::find_if(std::next(sectionIt), sectionEnd, [&](const std::string &l) { return l.rfind(keyPrefix, 0) == 0; });
        if (existing != sectionEnd) {
            *existing = assocLine;
        } else {
            lines.insert(sectionEnd, assocLine);
        }
    }

    const auto tmpPath = SyncPath(mimeAppsPath.string() + ".kdrive.tmp");
    {
        std::ofstream out{tmpPath, std::ios::trunc};
        if (!out.is_open()) return false;
        for (const auto &l: lines) out << l << '\n';
    }
    std::error_code ec;
    std::filesystem::rename(tmpPath, mimeAppsPath, ec);
    if (ec) {
        std::filesystem::remove(tmpPath, ec);
        return false;
    }
    return true;
}
} // namespace

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
    // Register kDrive as the handler for the `kdrive://` URL scheme used by the
    // OAuth flow. Without this, the OAuth redirect from the browser cannot
    // reach kDrive and login fails with errors like "Could not read file
    // kdrive://auth-desktop?..." (see issue #1627).
    //
    // We do three things, each independently best-effort:
    //   1. Write `~/.local/share/applications/<exe>.desktop` declaring the
    //      MIME type `x-scheme-handler/kdrive` and the right `Exec=` path
    //      (for AppImage builds this comes from the $APPIMAGE env var).
    //   2. Run `update-desktop-database` so the desktop entry shows up in
    //      the user-level MIME cache.
    //   3. Set the default association via `xdg-mime default ...`, with a
    //      direct edit of `~/.config/mimeapps.list` as fallback for systems
    //      where xdg-utils is missing or returns a non-zero status.
    const char *homePathEnv = std::getenv("HOME");
    if (!homePathEnv) {
        LOG_WARN(Log::instance()->getLogger(), "Path to HOME not found");
        return false;
    }
    const std::string homePath(homePathEnv);

    const auto urlSchemeDirPath = SyncPath(homePath + "/.local/share/applications");
    const std::string desktopFileName = std::string(APPLICATION_EXECUTABLE) + ".desktop";
    const auto urlSchemeFilePath = urlSchemeDirPath / desktopFileName;

    IoError ioError = IoError::Unknown;
    bool exists = false;
    if (!IoHelper::checkIfPathExists(urlSchemeDirPath, exists, ioError, IoHelper::PathCheckOption::Insensitive)) {
        LOGW_WARN(logger(), L"Error in IoHelper::checkIfPathExists:" << Utility::formatIoError(urlSchemeDirPath, ioError));
        return false;
    }
    if (!exists && !IoHelper::createDirectory(urlSchemeDirPath, false, ioError)) {
        LOGW_WARN(logger(),
                  L"Could not create login-redirection directory: " << Utility::formatIoError(urlSchemeDirPath, ioError));
        return false;
    }

    SyncPath execPath;
    const std::string appImageEnv = KDC::CommonUtility::envVarValue("APPIMAGE");
    if (!appImageEnv.empty()) {
        execPath = SyncPath(appImageEnv);
    } else {
        execPath = KDC::CommonUtility::getAppWorkingDir() / APPLICATION_EXECUTABLE;
    }

    {
        std::ofstream urlSchemeFile{urlSchemeFilePath, std::ios::trunc};
        if (!urlSchemeFile.is_open()) {
            LOGW_WARN(logger(), L"Could not open login-redirection desktop file for writing: "
                                        << Utility::formatSyncPath(urlSchemeFilePath));
            return false;
        }
        urlSchemeFile << "[Desktop Entry]\n"
                      << "Type=Application\n"
                      << "Name=" << APPLICATION_NAME << "\n"
                      << "GenericName=Folder Sync\n"
                      << "Comment=" << APPLICATION_NAME << " desktop synchronization client\n"
                      << "Icon=" << APPLICATION_ICON_NAME << "\n"
                      << "Exec=" << escapeDesktopExecArg(execPath.string()) << " %u\n"
                      << "Terminal=false\n"
                      << "Categories=Utility;\n"
                      << "MimeType=" << mimeType << ";\n"
                      << "NoDisplay=true\n"
                      << "StartupNotify=false\n";
    }

    bool res = true;

    // Update desktop database. Failure is non-fatal: `xdg-mime` below still
    // works because it reads the .desktop file we just wrote directly.
    const std::string updateDesktopDbCmd = "update-desktop-database " + shellQuote(urlSchemeDirPath.string());
    if (const int updateResult = system(updateDesktopDbCmd.c_str()); updateResult != 0) {
        LOGW_INFO(logger(), L"update-desktop-database not available or failed: " << CommonUtility::s2ws(updateDesktopDbCmd)
                                                                                 << L", result: " << updateResult);
        res = false;
    }

    // Register the scheme via xdg-mime, then verify with a direct mimeapps.list
    // edit as fallback. On some minimal/Wayland-only Arch derivatives (e.g.
    // CachyOS with Hyprland) the xdg-utils invocation succeeds but the
    // mimeapps.list entry is not picked up by browsers; writing it ourselves
    // guarantees the association.
    const std::string registerSchemeCmd = "xdg-mime default " + shellQuote(desktopFileName) + " " + shellQuote(mimeType);
    if (const int registerResult = system(registerSchemeCmd.c_str()); registerResult != 0) {
        LOGW_INFO(logger(), L"xdg-mime not available or failed: " << CommonUtility::s2ws(registerSchemeCmd) << L", result: "
                                                                  << registerResult);
        res = false;
    }

    const auto mimeAppsPath = SyncPath(homePath + "/.config/mimeapps.list");
    if (writeMimeAppsDefault(mimeAppsPath, desktopFileName, mimeType)) {
        LOGW_INFO(logger(), L"Registered " << CommonUtility::s2ws(mimeType) << L" -> " << CommonUtility::s2ws(desktopFileName)
                                           << L" in " << Utility::formatSyncPath(mimeAppsPath));
        // Direct write succeeded: even if xdg-mime failed, the scheme is set.
        res = true;
    } else {
        LOGW_WARN(logger(), L"Failed to write " << Utility::formatSyncPath(mimeAppsPath));
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


} // namespace KDC
