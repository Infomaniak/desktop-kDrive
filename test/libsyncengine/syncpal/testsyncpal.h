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

#include "testincludes.h"
#include "syncpal/syncpal.h"

using namespace CppUnit;

namespace KDC {

class TestSyncPal : public CppUnit::TestFixture {
        CPPUNIT_TEST_SUITE(TestSyncPal);
        CPPUNIT_TEST(testUpdateTree);
        CPPUNIT_TEST(testSnapshot);
        CPPUNIT_TEST(testCopySnapshots);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override;
        void tearDown() override;

    private:
        std::shared_ptr<SyncPal> _syncPal;
        std::shared_ptr<ParmsDb> _parmsDb;

        int _driveDbId;
        SyncPath _localPath;
        SyncPath _remotePath;

        void testUpdateTree();
        void testSnapshot();
        void testCopySnapshots();
        void testAll();
        void testConflictQueue();
        bool exec_case_6_4();
        bool check_case_6_4();
};

}  // namespace KDC
