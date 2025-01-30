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

#include "libcommon/utility/utility.h"

#include <fstream>


#ifdef _WIN32
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
    if (!Utility::normalizedSyncName(Str("ééé"), nfdNormalized, Utility::UnicodeNormalization::NFD)) {
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

void generateOrEditTestFile(const SyncPath& path) {
    std::ofstream testFile(path);
    testFile << "test" << std::endl;
    testFile.close();
}

std::string loadEnvVariable(const std::string& key) {
    const std::string val = KDC::CommonUtility::envVarValue(key);
    if (val.empty()) {
        std::cout << "Environment variables " << key << " is missing!" << std::endl;
        throw std::runtime_error("Environment variables " + key + " is missing!");
    }
    return val;
}
#ifdef _WIN32
void setModificationDate(const SyncPath& path, const std::chrono::time_point<std::chrono::system_clock>& timePoint) {
    struct _utimbuf timeBuffer;
    const std::time_t timeInSeconds = std::chrono::system_clock::to_time_t(timePoint);

    IoError ioError = IoError::Success;
    FileStat fileStat;
    ::KDC::IoHelper::getFileStat(path, &fileStat, ioError);

    timeBuffer.actime = fileStat.creationTime;
    timeBuffer.modtime = timeInSeconds;
    _wutime(path.wstring().c_str(), &timeBuffer);
}
#else
void setModificationDate(const SyncPath& path, const std::chrono::time_point<std::chrono::system_clock>& timePoint) {
    struct stat fileStat;
    struct utimbuf newTime;

    const auto fileNameStr = path.string();
    const auto fileName = fileNameStr.c_str();

    stat(fileName, &fileStat);

    const std::time_t timeInSeconds = std::chrono::system_clock::to_time_t(timePoint);
    newTime.modtime = timeInSeconds;
    utime(fileName, &newTime);
}
#endif
} // namespace KDC::testhelpers
