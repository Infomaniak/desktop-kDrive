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

#include "testhelpers.h"
#include "localtemporarydirectory.h"
#include "io/iohelper.h"

#include "libcommon/utility/utility.h"
#include "libcommonserver/io/iohelper.h"

#include <fstream>

#include <Poco/JSON/Object.h>

#include <regex>
#include <sys/stat.h>
#include <utime.h>

namespace KDC::testhelpers {

SyncPath removeNumericSuffix(const SyncPath &relativePath) {
    if (relativePath.empty()) return {};

    static const std::regex numericSuffixRegex(".*(\\.[0-9]+)$");

    std::list<SyncName> segments = CommonUtility::splitSyncPath(relativePath);
    auto &root = segments.front();

    std::smatch words;
    const std::string rootStr = SyncName2Str(root);
    (void) std::regex_match(rootStr, words, numericSuffixRegex);

    if (words.size() <= 1) return relativePath; // No numeric suffix.

    root = root.substr(0, rootStr.size() - std::string{words[1]}.size()); // Removes suffix from root directory name.

    // Adapt the relative path.
    std::stringstream ss;
    std::uint64_t i = 0;
    for (const auto &segment: segments) {
        if (i > 0) ss << "/";
        ss << SyncName2Str(segment);
        ++i;
    }

    return SyncPath{ss.str()};
}

void eraseFromTrash(const KDC::SyncPath &relativePath) {
    const auto trashPath = Utility::getTrashPath();
    std::error_code ec;

    auto dirIt = std::filesystem::recursive_directory_iterator(trashPath,
                                                               std::filesystem::directory_options::skip_permission_denied, ec);
    if (ec) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in testhelpers::eraseFromTrash: " << Utility::formatStdError(ec));
        return;
    }

    std::vector<SyncPath> itemsToErase;
    for (; dirIt != std::filesystem::recursive_directory_iterator(); ++dirIt) {
        const auto p = dirIt->path();
        const auto dirItemRelativePath = std::filesystem::relative(dirIt->path(), trashPath, ec);
        // Filter out the numerical suffix of the root directory name, e.g: `dirname.15` is replaced with `dirname`.
        const auto suffixFreeDirectorEntryPath = removeNumericSuffix(dirItemRelativePath);
        if (relativePath == suffixFreeDirectorEntryPath) itemsToErase.push_back(dirIt->path());
    }

    for (const auto &pathToErase: itemsToErase) (void) IoHelper::deleteItem(pathToErase);
}

bool isInTrash(const SyncPath &relativePath) {
    const auto trashPath = Utility::getTrashPath();
    std::error_code ec;

    auto dirIt = std::filesystem::recursive_directory_iterator(trashPath,
                                                               std::filesystem::directory_options::skip_permission_denied, ec);
    if (ec) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in testhelpers::isInTrash: " << Utility::formatStdError(ec));
        return false;
    }

    for (; dirIt != std::filesystem::recursive_directory_iterator(); ++dirIt) {
        const auto dirItemRelativePath = std::filesystem::relative(dirIt->path(), trashPath, ec);
        // Filter out the numerical suffix of the root directory name, e.g.: `dirname.15` is replaced with `dirname`.
        const auto suffixFreedirectorEntryPath = removeNumericSuffix(dirItemRelativePath);
        if (relativePath == suffixFreedirectorEntryPath) return true;
    }

    return false;
}

} // namespace KDC::testhelpers
