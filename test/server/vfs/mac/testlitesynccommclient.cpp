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

#include "testlitesynccommclient.h"

#include "comm/litesynccommclient_mac.h"

using namespace CppUnit;

namespace KDC {

TestLiteSyncCommClient::TestLiteSyncCommClient() :
    CppUnit::TestFixture() {}

void TestLiteSyncCommClient::setUp() {
    TestBase::start();
}

void TestLiteSyncCommClient::tearDown() {
    TestBase::stop();
}

void TestLiteSyncCommClient::testGetVfsStatus() {
    /*
    bool isPlaceholder = false;
    bool isHydrated = false;
    bool isSyncing = false;
    int progress = 0;

    // vfsGetStatus returns `true` if the file path indicates a non-existing item.
    log4cplus::Logger logger;
    CPPUNIT_ASSERT(LiteSyncCommClient::vfsGetStatus("this_file_does_not_exist.txt", isPlaceholder, isHydrated, isSyncing,
                                                      progress, logger));
    */
}

} // namespace KDC
