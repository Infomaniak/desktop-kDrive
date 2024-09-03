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
#include <fstream>
#include "testhelpers.h"

#include "utility/utility.h"

namespace KDC::testhelpers {

SyncName makeNfdSyncName() {
#ifdef _WIN32
    return Utility::normalizedSyncName(L"ééé", Utility::UnicodeNormalization::NFD);
#else
    return Utility::normalizedSyncName("ééé", Utility::UnicodeNormalization::NFD);
#endif
}

SyncName makeNfcSyncName() {
#ifdef _WIN32
    return Utility::normalizedSyncName(L"ééé");
#else
    return Utility::normalizedSyncName("ééé");
#endif
}

void generateOrEditTestFile(const SyncPath& path) {
    std::ofstream testFile(path);
    testFile << "test" << std::endl;
    testFile.close();
}

std::string loadEnvVariable(const std::string& key) {
    const std::string val = KDC::CommonUtility::envVarValue(key);
    if (val.empty()) {
        throw std::runtime_error("Environment variables " + key + " is missing!");
    }
    return val;
}

} // namespace KDC::testhelpers
