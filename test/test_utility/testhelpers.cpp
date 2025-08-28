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

#include "libcommon/utility/utility.h"

#include <fstream>
#include <Poco/JSON/Object.h>


#if defined(KD_WINDOWS)
#include "libcommonserver/io/filestat.h"
#include "libcommonserver/io/iohelper.h"
#include <sys/utime.h>
#include <sys/types.h>
#else
#include <sys/stat.h>
#include <utime.h>
#endif

namespace KDC::testhelpers {

SyncName makeNfdSyncName() {
    SyncName nfdNormalized;
    if (!Utility::normalizedSyncName(Str("ééé"), nfdNormalized, UnicodeNormalization::NFD)) {
        assert(false);
    }
    return nfdNormalized;
}

SyncName makeNfcSyncName() {
    SyncName nfcNormalized;
    if (!Utility::normalizedSyncName(Str("ééé"), nfcNormalized)) {
        assert(false);
    }
    return nfcNormalized;
}

void generateOrEditTestFile(const SyncPath &path) {
    std::ofstream testFile(path, std::ios::app);
    testFile << "test" << std::endl;
    testFile.close();
}

void generateBigFiles(const SyncPath &dirPath, const uint16_t size, const uint16_t count) {
    // Generate the 1st big file
    const auto bigFilePath = generateBigFile(dirPath, size);

    // Generate others big files
    for (uint16_t i = 1; i < count; i++) {
        std::stringstream fileName;
        fileName << "big_file_" << size << "_" << i << ".txt";
        const SyncPath newBigFilePath = SyncPath(dirPath) / fileName.str();
        (void) std::filesystem::copy_file(bigFilePath, newBigFilePath, std::filesystem::copy_options::overwrite_existing);
    }
}

SyncPath generateBigFile(const SyncPath &dirPath, const uint16_t size) {
    std::stringstream fileName;
    fileName << "big_file_" << size << "_" << 0 << ".txt";
    const std::string str{"0123456789"};
    const auto bigFilePath = SyncPath(dirPath) / fileName.str();

    std::ofstream ofs(bigFilePath, std::ios_base::in | std::ios_base::trunc);
    for (uint64_t i = 0;
         i < static_cast<uint64_t>(round(static_cast<double>(size * 1000000) / static_cast<double>(str.length()))); i++) {
        ofs << str;
    }
    return bigFilePath;
}

std::string loadEnvVariable(const std::string &key, const bool mandatory) {
    const std::string val = KDC::CommonUtility::envVarValue(key);
    if (val.empty() && mandatory) {
        std::cout << "Environment variables " << key << " is missing!" << std::endl;
        throw std::runtime_error("Environment variables " + key + " is missing!");
    }
    return val;
}

void createSymLinkLoop(const SyncPath &filepath1, const SyncPath &filepath2, const NodeType nodeType) {
    LocalTemporaryDirectory tempDir;
    const auto currentPath = std::filesystem::current_path();
    std::filesystem::current_path(tempDir.path());

    switch (nodeType) {
        case NodeType::File: {
            std::ofstream ofs("filepath1");
            break;
        }
        case NodeType::Directory: {
            std::filesystem::create_directories("filepath1");
            break;
        }
        default:
            throw std::invalid_argument(
                    "Invalid argument NodeType argument in createSymLinkLoop. Expected: either NodeType::File or "
                    "NodeType::Directory.");
    }

    std::filesystem::create_symlink(filepath1.filename(), filepath2.filename()); // filepath2 -> filepath1

    std::filesystem::current_path(currentPath);
    std::filesystem::rename(tempDir.path() / filepath2.filename(), filepath2);

    std::filesystem::current_path(tempDir.path());
    std::filesystem::remove_all(filepath1.filename());
    std::filesystem::remove_all(filepath2.filename());
    std::error_code ec;
    std::filesystem::create_symlink(filepath2.filename(), filepath1.filename(), ec); // filepath1 -> filepath2

    std::filesystem::current_path(currentPath);
    std::filesystem::rename(tempDir.path() / filepath1.filename(), filepath1);
}

} // namespace KDC::testhelpers
