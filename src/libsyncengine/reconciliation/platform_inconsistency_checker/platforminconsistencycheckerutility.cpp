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

#include "platforminconsistencycheckerutility.h"

#include "libcommon/utility/utility.h"
#include "jobs/local/localmovejob.h"

#include <iomanip>
#include <log4cplus/loggingmacros.h>
#include <unordered_set>

#if defined(KD_WINDOWS)
#include "libcommonserver/utility/utility.h"
#include <Poco/Util/WinRegistryKey.h>
#endif

namespace forbidden_filename_characters {
static const char fat32[] = {'\\', '/', ':', '*', '?', '"', '<', '>', '|', '\n', '\r', '\t', '\0'};

#if defined(KD_WINDOWS)
static const chars[] = {'\\', '/', ':', '*', '?', '"', '<', '>', '|', '\n'};
#else
#if defined(KD_MACOS)
static const char chars[] = {'/'};
#else
static const char chars[] = {'/', '\0'};
#endif
#endif
} // namespace forbidden_filename_characters

static const int maxNameLengh = 255; // Max filename length is uniformized to 255 characters for all platforms and backends

namespace KDC {

#if defined(KD_WINDOWS)
static const std::unordered_set<std::string> reservedWinNames = {
        "CON",  "PRN",  "AUX",  "NUL",  "CON",  "PRN",  "AUX",  "NUL",  "COM1", "COM2", "COM3", "COM4", "COM5",
        "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};
#endif

std::shared_ptr<PlatformInconsistencyCheckerUtility> PlatformInconsistencyCheckerUtility::_instance = nullptr;
size_t PlatformInconsistencyCheckerUtility::_maxPathLength = 0;

std::shared_ptr<PlatformInconsistencyCheckerUtility> PlatformInconsistencyCheckerUtility::instance() {
    if (_instance == nullptr) {
        try {
            _instance = std::shared_ptr<PlatformInconsistencyCheckerUtility>(new PlatformInconsistencyCheckerUtility());
        } catch (...) {
            assert(false);
            return nullptr;
        }
    }
    return _instance;
}

SyncName PlatformInconsistencyCheckerUtility::generateNewValidName(const SyncPath &name, SuffixType suffixType) {
    SyncName suffix = generateSuffix(suffixType);
    const SyncName sub = name.stem().native().substr(0, maxNameLengh - suffix.size() - name.extension().native().size());

#if defined(KD_WINDOWS)
    SyncName nameStr(name.native());
    // Can't finish with a space or a '.'
    if (nameStr.back() == ' ' || nameStr.back() == '.') {
        return sub + name.extension().native() + suffix;
    }
#endif

    return sub + suffix + name.extension().native();
}

ExitInfo PlatformInconsistencyCheckerUtility::renameLocalFile(const SyncPath &absoluteLocalPath, SuffixType suffixType,
                                                              SyncPath *newPathPtr /*= nullptr*/) {
    const auto newName = PlatformInconsistencyCheckerUtility::instance()->generateNewValidName(absoluteLocalPath, suffixType);
    auto newFullPath = absoluteLocalPath.parent_path() / newName;

    LocalMoveJob moveJob(absoluteLocalPath, newFullPath);
    moveJob.runSynchronously();

    if (newPathPtr) {
        *newPathPtr = std::move(newFullPath);
    }

    return moveJob.exitInfo();
}

ExitInfo PlatformInconsistencyCheckerUtility::nameHasForbiddenChars(
        const SyncName &name, [[maybe_unused]] std::shared_ptr<CacheDirectory> cacheDirectory, bool &hasForbiddenChars) {
    hasForbiddenChars = false;
    std::string forbiddenChars;

    const auto exitInfo = forbiddenFilenameChars(cacheDirectory, forbiddenChars);
    if (!exitInfo) return exitInfo;

    for (auto c: forbiddenChars) {
        if (name.find(c) != std::string::npos) {
            LOGW_INFO(Log::instance()->getLogger(),
                      L"Name '" << SyncName2WStr(name) << L"' contains forbidden character: '" << std::wstring(1, c) << L"'");
            hasForbiddenChars = true;
            return ExitCode::Ok;
        }
    }

#if defined(KD_WINDOWS)
    // Check for forbidden ascii codes
    for (const SyncChar c: name) {
        const int64_t asciiCode(c);
        if (asciiCode <= 31) {
            LOGW_INFO(Log::instance()->getLogger(),
                      L"Name '" << SyncName2WStr(name) << L"' contains forbidden character: '" << std::wstring(1, c) << L"'");
            forbiddenChars = true;
        }
    }
#endif

    return ExitCode::Ok;
}

bool PlatformInconsistencyCheckerUtility::isNameOnlySpaces(const SyncName &name) {
    return CommonUtility::ltrim(name).empty();
}

ExitInfo PlatformInconsistencyCheckerUtility::checkIfNameEndWithForbiddenSpace(
        [[maybe_unused]] const SyncName &name, [[maybe_unused]] const std::shared_ptr<CacheDirectory> cacheDirectory,
        bool &endsWithForbiddenSpace) {
    endsWithForbiddenSpace = false;
#if defined(KD_WINDOWS)
    // Can't finish with a space
    if (SyncName nameStr(name); nameStr[nameStr.size() - 1] == ' ') {
        endsWithForbiddenSpace = true;
    }
#elif defined(KD_LINUX)
    SyncPath localSyncPath;
    if (const auto exitInfo = cacheDirectory->path(localSyncPath); !exitInfo) return exitInfo;

    if (!CommonUtility::isEXT234(localSyncPath)) return ExitCode::Ok;

    // `statfs` can confuse more restrictive systems with EXT2/3/4 when a USB stick is used.
    bool fileNamesCanEndWithSpace = true;
    if (const auto exitInfo = Utility::checkIfFileNamesCanEndWithSpace(cacheDirectory, fileNamesCanEndWithSpace); !exitInfo)
        return exitInfo;

    endsWithForbiddenSpace = name.back() == ' ';
#endif
    return ExitCode::Ok; // Name ending with a space is only forbidden on Windows.
}

#if defined(KD_WINDOWS)
bool PlatformInconsistencyCheckerUtility::fixNameWithBackslash(const SyncName &name, SyncName &newName) {
    size_t pos = name.find('\\');
    while (pos != std::string::npos) {
        if (newName.empty()) {
            newName = name; // Copy filename to newName only if necessary
        }

        newName.replace(pos, 1, charToHex(newName[pos]));
        pos = newName.find('\\', pos);
    }

    return newName != name && !newName.empty();
}
#endif

bool PlatformInconsistencyCheckerUtility::isNameTooLong(const SyncName &name) const {
    return SyncName2Str(name).size() > maxNameLengh;
}

// return false if the file name is ok
// true if the file name has been fixed
bool PlatformInconsistencyCheckerUtility::checkReservedNames(const SyncName &name) {
    if (name.empty()) {
        return false;
    }

    if (name == Str("..") || name == Str(".")) {
        LOGW_INFO(Log::instance()->getLogger(), L"Items named '.' or '..' are forbidden on the filesystem");
        return true;
    }

#if defined(KD_WINDOWS)
    // Can't have only dots
    if (std::ranges::count(name, '.') == name.size()) {
        LOGW_INFO(Log::instance()->getLogger(), L"Names containing only dots are forbidden on the filesystem");
        return true;
    }

    // Can't finish with a '.'
    if (name[name.size() - 1] == '.') {
        LOGW_INFO(Log::instance()->getLogger(),
                  L"Names ending with a dot are forbidden on the filesystem: " << Utility::formatSyncName(name));
        return true;
    }

    for (const auto &reserved: reservedWinNames) {
        if (CommonUtility::startsWithInsensitive(name, Str2SyncName(reserved)) && name.size() == reserved.size()) {
            LOGW_INFO(Log::instance()->getLogger(), L"Name '" << Str2SyncName(reserved) << L"' is reserved on the filesystem");
            return true;
        }
    }
#endif

    return false;
}

bool PlatformInconsistencyCheckerUtility::isPathTooLong(size_t pathSize) {
    return pathSize > _maxPathLength;
}

PlatformInconsistencyCheckerUtility::PlatformInconsistencyCheckerUtility() {
    setMaxPath();
}

SyncName PlatformInconsistencyCheckerUtility::charToHex(unsigned int c) {
    OStringStream stm;
    stm << '%' << std::hex << c;
    return stm.str();
}

void PlatformInconsistencyCheckerUtility::setMaxPath() {
    _maxPathLength = CommonUtility::maxPathLength();
}

SyncName PlatformInconsistencyCheckerUtility::generateSuffix(SuffixType suffixType) {
    std::time_t now = std::time(nullptr);
    std::tm tm = *std::localtime(&now);

    SyncName suffix;
    StringStream ss;
    ss << std::put_time(&tm, Str("%Y%m%d_%H%M%S"));

    switch (suffixType) {
        case SuffixType::Conflict:
            suffix = Str("_conflict_");
            break;
        case SuffixType::Orphan:
            suffix = Str("_orphan_");
            break;
        case SuffixType::Blacklisted:
            suffix = Str("_blacklisted_");
            break;
    }

    return suffix + ss.str() + Str("_") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10));
}

ExitInfo PlatformInconsistencyCheckerUtility::forbiddenFilenameChars(
        [[maybe_unused]] const std::shared_ptr<CacheDirectory> cacheDirectory, std::string &forbiddenChars) {
    forbiddenChars = forbidden_filename_characters::chars;
#if defined(KD_LINUX)
    std::string fileSystemName;
    const auto exitInfo = Utility::getFileSystemName(cacheDirectory, fileSystemName);
    if (!exitInfo) return exitInfo;
    if (fileSystemName == CommonUtility::exFAT()) forbiddenChars = forbidden_filename_characters::fat32;
#endif
    return ExitCode::Ok;
}

} // namespace KDC
