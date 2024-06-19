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

#ifdef WIN32
static const std::unordered_set<std::string> reservedWinNames = {
    "CON",  "PRN",  "AUX",  "NUL",  "CON",  "PRN",  "AUX",  "NUL",  "COM1", "COM2", "COM3", "COM4", "COM5",
    "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};
#endif

std::shared_ptr<PlatformInconsistencyCheckerUtility> PlatformInconsistencyCheckerUtility::_instance = nullptr;
size_t PlatformInconsistencyCheckerUtility::_maxPathLength = 0;
#if defined(_WIN32)
size_t PlatformInconsistencyCheckerUtility::_maxPathLengthFolder = 0;
#endif

std::shared_ptr<PlatformInconsistencyCheckerUtility> PlatformInconsistencyCheckerUtility::instance() {
    if (_instance == nullptr) {
        _instance = std::shared_ptr<PlatformInconsistencyCheckerUtility>(new PlatformInconsistencyCheckerUtility());
    }
    return _instance;
}

SyncName PlatformInconsistencyCheckerUtility::generateNewValidName(const SyncPath &name,
                                                                   SuffixType suffixType) {
    SyncName suffix = generateSuffix(suffixType);
    SyncName sub = name.stem().native().substr(0, MAX_NAME_LENGTH - suffix.size() - name.extension().native().size());

#ifdef WIN32
    SyncName nameStr(name.native());
    // Can't finish with a space or a '.'
    if (nameStr.back() == ' ' || nameStr.back() == '.') {
        return sub + name.extension().native() + suffix;
    }
#endif

    return sub + suffix + name.extension().native();
}

ExitCode PlatformInconsistencyCheckerUtility::renameLocalFile(const SyncPath &absoluteLocalPath, SuffixType suffixType, SyncPath *newPathPtr /*= nullptr*/) {
    const auto newName = PlatformInconsistencyCheckerUtility::instance()->generateNewValidName(
        absoluteLocalPath, suffixType);
    auto newFullPath = absoluteLocalPath.parent_path() / newName;

    LocalMoveJob moveJob(absoluteLocalPath, newFullPath);
    moveJob.runSynchronously();

    if (newPathPtr) {
        *newPathPtr = std::move(newFullPath);
    }

    return moveJob.exitCode();
}

bool PlatformInconsistencyCheckerUtility::fixNameForbiddenChars(const SyncPath &name, SyncName &newName) {
    // Does the string contain one of the characters?
    for (auto c : forbiddenFilenameChars) {
        size_t pos = newName.empty() ? name.native().find(c) : newName.find(c);
        if (newName.empty() && pos != std::string::npos) {
            newName = name.native();  // Copy filename to newName only if necessary
        }

        while (pos != std::string::npos) {
            newName.replace(pos, 1, charToHex(newName[pos]));
            pos = newName.find(c, pos);
        }
    }

    return newName != name && !newName.empty();
}

bool PlatformInconsistencyCheckerUtility::checkNameForbiddenChars(const SyncPath &name) {
    for (auto c : forbiddenFilenameChars) {
        if (name.native().find(c) != std::string::npos) {
            return true;
        }
    }

#ifdef _WIN32
    // Can't finish with a space
    SyncName nameStr(name.native());
    if (nameStr[nameStr.size() - 1] == ' ') {
        return true;
    }

    // Check for forbidden ascii codes
    for (wchar_t c : name.native()) {
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
            newName = name;  // Copy filename to newName only if necessary
        }

        newName.replace(pos, 1, charToHex(newName[pos]));
        pos = newName.find('\\', pos);
    }

    return newName != name && !newName.empty();
}
#endif

bool PlatformInconsistencyCheckerUtility::checkNameSize(const SyncPath &name) {
    if (name.native().size() > MAX_NAME_LENGTH) {
        return true;
    }

    return false;
}

// return false if the file name is ok
// true if the file name has been fixed
bool PlatformInconsistencyCheckerUtility::checkReservedNames(const SyncPath &name) {
    if (name.empty()) {
        return false;
    }

    SyncName nameStr(name.native());
    if (nameStr == Str("..") || nameStr == Str(".")) {
        return true;
    }

#ifdef WIN32
    // Can't have only dots
    size_t dotCount = std::count(nameStr.begin(), nameStr.end(), '.');
    if (dotCount == nameStr.size()) {
        return true;
    }

    // Can't finish with a '.'
    if (nameStr[nameStr.size() - 1] == '.') {
        return true;
    }

    for (const std::string &reserved : reservedWinNames) {
        if (Utility::startsWithInsensitive(nameStr, Str2SyncName(reserved))) {
            if (nameStr.size() == reserved.size()) {
                return true;
            }
        }
    }
#endif

    return false;
}

bool PlatformInconsistencyCheckerUtility::checkPathLength(size_t pathSize, NodeType type) {
    size_t maxLength = _maxPathLength;

#ifdef _WIN32
    if (type == NodeType::Directory) {
        maxLength = _maxPathLengthFolder;
    }
#else
    (void)type;
#endif

    return pathSize > maxLength;
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
#if defined(_WIN32)
    _maxPathLengthFolder = CommonUtility::maxPathLengthFolder();
#endif
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

}  // namespace KDC
