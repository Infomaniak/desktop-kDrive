
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

#include "testabstractupdater.h"

#include <regex>

#include "server/updater_v2/abstractupdater.h"

namespace KDC {
void TestAbstractUpdater::setUp() {
}

void TestAbstractUpdater::tearDown() {
}

void TestAbstractUpdater::testCurrentVersion() {
    std::string test = AbstractUpdater::currentVersion();
#ifdef NDEBUG
    CPPUNIT_ASSERT(
            std::regex_match(test, std::regex(R"(\d{1,2}[.]\d{1,2}[.]\d{1,2}[.]\d{8}$)")));
#else
    CPPUNIT_ASSERT(
        std::regex_match(test, std::regex(R"(\d{1,2}[.]\d{1,2}[.]\d{1,2}[.]0$)")));
#endif
}
} // KDC
