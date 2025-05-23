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

#include "testincludes.h"


#ifdef __APPLE__
#include "vfs/mac/testlitesyncextconnector.h"
#include "vfs/mac/testvfsmac.h"
#endif
#include "workers/testworkers.h"
#include "updater/testabstractupdater.h"
#include "updater/testupdatechecker.h"
#include "requests/testserverrequests.h"
#include "appserver/testappserver.h"

namespace KDC {

#ifdef __APPLE__
CPPUNIT_TEST_SUITE_REGISTRATION(TestVfsMac);
CPPUNIT_TEST_SUITE_REGISTRATION(TestLiteSyncExtConnector);
#endif
CPPUNIT_TEST_SUITE_REGISTRATION(TestWorkers);
CPPUNIT_TEST_SUITE_REGISTRATION(TestUpdateChecker);
CPPUNIT_TEST_SUITE_REGISTRATION(TestAbstractUpdater);
CPPUNIT_TEST_SUITE_REGISTRATION(TestServerRequests);
CPPUNIT_TEST_SUITE_REGISTRATION(TestAppServer);
} // namespace KDC

int main(int, char **) {
    return runTestSuite("_kDriveTestServer.log");
}
