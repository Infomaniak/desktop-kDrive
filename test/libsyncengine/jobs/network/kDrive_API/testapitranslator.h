/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#pragma once

#include "testincludes.h"
#include "keychainmanager/apitoken.h"
#include "test_utility/localtemporarydirectory.h"

#include "utility/types.h"
#include "libcommonserver/io/iohelper.h"
using namespace CppUnit;

namespace KDC {
class TestApiTranslator : public CppUnit::TestFixture, public TestBase {
    public:
        CPPUNIT_TEST_SUITE(TestApiTranslator);
        CPPUNIT_TEST(testGetDriveDbId);
        CPPUNIT_TEST(testTranslateV2ToV3);
        CPPUNIT_TEST(testTranslateV3ToV2);
        CPPUNIT_TEST(testGetSpecialFolders);
        CPPUNIT_TEST(translateRemoteNodeInfoListFromV3ToV2);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override;
        void tearDown() override;

    private:
        void testGetDriveDbId();
        void testTranslateV2ToV3();
        void testTranslateV3ToV2();
        void translateRemoteNodeInfoListFromV3ToV2();
        void testGetSpecialFolders();

        DriveId _driveId = 0;
        DriveDbId _driveDbId = 0;
        UserDbId _userDbId = 0;
        RemoteNodeId _remoteDirId;
        ApiToken _apiToken;

        LocalTemporaryDirectory _localParmsDbTempDir{"testApiTranslator"};
};
} // namespace KDC
