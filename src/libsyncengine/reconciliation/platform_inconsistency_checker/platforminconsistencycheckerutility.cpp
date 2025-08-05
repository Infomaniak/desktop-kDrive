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

#if defined(KD_WINDOWS)
static const char forbiddenFilenameChars[] = {'\\', '/', ':', '*', '?', '"', '<', '>', '|', '\n'};
#else
#if defined(KD_MACOS)
static const char forbiddenFilenameChars[] = {'/'};
#else
static const char forbiddenFilenameChars[] = {'/', '\0'};
#endif
#endif

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

bool PlatformInconsistencyCheckerUtility::nameHasForbiddenChars(const SyncPath &name) {
    for (auto c: forbiddenFilenameChars) {
        if (name.native().find(c) != std::string::npos) {
            return true;
        }
    }

#if defined(KD_WINDOWS)
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

bool PlatformInconsistencyCheckerUtility::isNameOnlySpaces(const SyncName &name) {
    return Utility::ltrim(name).empty();
}

bool PlatformInconsistencyCheckerUtility::nameEndWithForbiddenSpace([[maybe_unused]] const SyncName &name) {
#if defined(KD_WINDOWS)
    // Can't finish with a space
    if (SyncName nameStr(name); nameStr[nameStr.size() - 1] == ' ') {
        return true;
    }
#endif
    return false; // Name ending with a space is only forbidden on Windows.
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
        return true;
    }

#if defined(KD_WINDOWS)
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

} // namespace KDC
