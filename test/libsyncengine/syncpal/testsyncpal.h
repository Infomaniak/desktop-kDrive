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
#pragma once
#include "testincludes.h"
#include "test_utility/localtemporarydirectory.h"

#include "syncpal/syncpal.h"

using namespace CppUnit;

namespace KDC {

class TestSyncPal : public CppUnit::TestFixture, public TestBase {
        CPPUNIT_TEST_SUITE(TestSyncPal);
        CPPUNIT_TEST(testUpdateTree);
        CPPUNIT_TEST(testSnapshot);
        CPPUNIT_TEST(testCopySnapshots);
        CPPUNIT_TEST(testOperationSet);
        CPPUNIT_TEST(testSyncFileItem);
        CPPUNIT_TEST(testCheckIfExistsOnServer);
        CPPUNIT_TEST(testBlacklist);
        CPPUNIT_TEST_SUITE_END();

    public:
        TestSyncPal() {}
        void setUp() override;
        void tearDown() override;

    private:
        std::shared_ptr<SyncPal> _syncPal;
        std::shared_ptr<ParmsDb> _parmsDb;
        LocalTemporaryDirectory _localTempDir = LocalTemporaryDirectory("TestSyncpal");
        int _driveDbId;
        SyncPath _localPath;
        SyncPath _remotePath;

        void testUpdateTree();
        void testSnapshot();
        void testOperationSet();
        void testCopySnapshots();
        void testSyncFileItem();
        void testCheckIfExistsOnServer();
        void testBlacklist();

        void testAll();
        void testConflictQueue();
        bool exec_case_6_4();
        bool check_case_6_4();
};

} // namespace KDC
