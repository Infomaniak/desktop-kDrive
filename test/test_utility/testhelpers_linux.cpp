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
namespace {
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

enum class TrashSubDirectory {
    Info,
    Files
};

// Get the user trash subdirectory
SyncPath getTrashSubDir(const TrashSubDirectory trashSubDir) {
    switch (trashSubDir) {
        case TrashSubDirectory::Info:
            return SyncPath{Utility::getTrashPath()}.parent_path().parent_path() / "info";
            break;
        case TrashSubDirectory::Files:
            return Utility::getTrashPath();
            break;
        default:
            throw std::invalid_argument("Unrecognized trash subdirectory type.");
    }

    return {};
}


// Parse the mandatory `Path` entry from a .trashinfo file
std::string getOriginalPath(const SyncPath &infoFile) {
    std::ifstream file(infoFile);
    if (!file.is_open()) return "";

    std::string line;
    static const std::string pathKey = "Path=";
    while (std::getline(file, line)) {
        if (line.rfind(pathKey, 0) != 0) continue;
        return line.substr(pathKey.size());
    }

    return "";
}

bool isSubPath(const SyncPath &path1, const SyncPath &path2) {
    try {
        const SyncPath canonicalPath1 = std::filesystem::weakly_canonical(path1);
        const SyncPath canonicalPath2 = std::filesystem::weakly_canonical(path2);

        const auto [path1It, path2It] =
                std::mismatch(canonicalPath1.begin(), canonicalPath1.end(), canonicalPath2.begin(), canonicalPath2.end());

        return (path2It == canonicalPath2.end());
    } catch (const std::filesystem::filesystem_error &e) {
        std::cerr << "Filesystem error in isSubPath: " << e.what() << std::endl;
        return false;
    }
}

} // namespace

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
        const auto suffixFreedirectorEntryPath = removeNumericSuffix(dirItemRelativePath);
        if (relativePath == suffixFreedirectorEntryPath) itemsToErase.push_back(dirIt->path());
    }

    auto ioError = IoError::Success;
    for (const auto &pathToErase: itemsToErase) (void) IoHelper::deleteItem(pathToErase, ioError);
}

// Check if a file is in the trash
bool isInTrash(const SyncPath &absoluteFilePath) {
    try {
        const SyncPath trashInfoDir = getTrashSubDir(TrashSubDirectory::Info);
        if (!std::filesystem::exists(trashInfoDir)) return false;

        const SyncPath targetPath = std::filesystem::absolute(absoluteFilePath);

        for (const auto &entry: std::filesystem::directory_iterator(trashInfoDir)) {
            if (entry.path().extension() != ".trashinfo") continue;

            const std::string originalPathStr = getOriginalPath(entry.path());
            if (originalPathStr.empty()) continue;

            const auto originalPath = std::filesystem::absolute(originalPathStr);
            if (!isSubPath(absoluteFilePath, originalPath)) continue;

            const SyncPath relativePath = std::filesystem::relative(absoluteFilePath, originalPath);
            if (relativePath.begin() == relativePath.end()) return true;

            const auto fileTrashParentName = entry.path().filename().stem();
            return std::filesystem::exists(getTrashSubDir(TrashSubDirectory::Files) / fileTrashParentName / relativePath);
        }
    } catch (const std::filesystem::filesystem_error &e) {
        std::cerr << "File system exception caught in `isInTrash`: " << e.what() << std::endl;
    }

    return false;
}


} // namespace KDC::testhelpers
