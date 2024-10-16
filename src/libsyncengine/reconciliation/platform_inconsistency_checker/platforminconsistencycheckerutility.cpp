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

#include "platforminconsistencycheckerutility.h"

#include "libcommon/utility/utility.h"
#include "jobs/local/localmovejob.h"

#include <iomanip>
#include <log4cplus/loggingmacros.h>
#include <unordered_set>

#ifdef _WIN32
#include "libcommonserver/utility/utility.h"
#include <Poco/Util/WinRegistryKey.h>
#endif

#ifdef _WIN32
static const char forbiddenFilenameChars[] = {'\\', '/', ':', '*', '?', '"', '<', '>', '|', '\n'};
#else
#ifdef __APPLE__
static const char forbiddenFilenameChars[] = {'/'};
#else
static const char forbiddenFilenameChars[] = {'/', '\0'};
#endif
#endif

#define MAX_NAME_LENGTH 255

namespace KDC {

#ifdef _WIN32
static const std::unordered_set<std::string> reservedWinNames = {
        "CON",  "PRN",  "AUX",  "NUL",  "CON",  "PRN",  "AUX",  "NUL",  "COM1", "COM2", "COM3", "COM4", "COM5",
        "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};
#endif

std::shared_ptr<PlatformInconsistencyCheckerUtility> PlatformInconsistencyCheckerUtility::_instance = nullptr;
size_t PlatformInconsistencyCheckerUtility::_maxPathLength = 0;

std::shared_ptr<PlatformInconsistencyCheckerUtility> PlatformInconsistencyCheckerUtility::instance() noexcept {
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
    SyncName sub = name.stem().native().substr(0, MAX_NAME_LENGTH - suffix.size() - name.extension().native().size());

#ifdef _WIN32
    SyncName nameStr(name.native());
    // Can't finish with a space or a '.'
    if (nameStr.back() == ' ' || nameStr.back() == '.') {
        return sub + name.extension().native() + suffix;
    }
#endif

    return sub + suffix + name.extension().native();
}

ExitCode PlatformInconsistencyCheckerUtility::renameLocalFile(const SyncPath &absoluteLocalPath, SuffixType suffixType,
                                                              SyncPath *newPathPtr /*= nullptr*/) {
    const auto newName = PlatformInconsistencyCheckerUtility::instance()->generateNewValidName(absoluteLocalPath, suffixType);
    auto newFullPath = absoluteLocalPath.parent_path() / newName;

    LocalMoveJob moveJob(absoluteLocalPath, newFullPath);
    moveJob.runSynchronously();

    if (newPathPtr) {
        *newPathPtr = std::move(newFullPath);
    }

    return moveJob.exitCode();
}

bool PlatformInconsistencyCheckerUtility::checkNameForbiddenChars(const SyncPath &name) {
    for (auto c: forbiddenFilenameChars) {
        if (name.native().find(c) != std::string::npos) {
            return true;
        }
    }

#ifdef _WIN32
    // Can't finish with a space
    if (SyncName nameStr(name.native()); nameStr[nameStr.size() - 1] == ' ') {
        return true;
    }

    // Check for forbidden ascii codes
    for (wchar_t c: name.native()) {
        int asciiCode(c);
        if (asciiCode <= 31) {
            return true;
        }
    }
#endif

    return false;
}

#ifdef _WIN32
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

bool PlatformInconsistencyCheckerUtility::checkNameSize(const SyncName &name) {
    if (name.size() > MAX_NAME_LENGTH) {
        return true;
    }

    return false;
}

// return false if the file name is ok
// true if the file name has been fixed
bool PlatformInconsistencyCheckerUtility::checkReservedNames(const SyncName &name) {
    if (name.empty()) {
        return false;
    }

    if (name == Str("..") || name == Str(".")) {
        return true;
    }

#ifdef _WIN32
    // Can't have only dots
    if (std::ranges::count(name, '.') == name.size()) {
        return true;
    }

    // Can't finish with a '.'
    if (name[name.size() - 1] == '.') {
        return true;
    }

    for (const auto &reserved: reservedWinNames) {
        if (Utility::startsWithInsensitive(name, Str2SyncName(reserved)) && name.size() == reserved.size()) {
            return true;
        }
    }
#endif

    return false;
}

bool PlatformInconsistencyCheckerUtility::checkPathLength(size_t pathSize) {
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

SyncName PlatformInconsistencyCheckerUtility::generateSuffix(SuffixType suffixType /*= SuffixTypeRename*/) {
    std::time_t now = std::time(nullptr);
    std::tm tm = *std::localtime(&now);

    SyncName suffix;
    StringStream ss;
    ss << std::put_time(&tm, Str("%Y%m%d_%H%M%S"));

    switch (suffixType) {
        case SuffixTypeRename:
            suffix = Str("_renamed_");
            break;
        case SuffixTypeConflict:
            suffix = Str("_conflict_");
            break;
        case SuffixTypeOrphan:
            suffix = Str("_orphan_");
            break;
        case SuffixTypeBlacklisted:
            suffix = Str("_blacklisted_");
            break;
    }

    return suffix + ss.str() + Str("_") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10));
}

} // namespace KDC
