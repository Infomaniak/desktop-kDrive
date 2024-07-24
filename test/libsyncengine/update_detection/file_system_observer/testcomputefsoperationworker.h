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

#pragma once

#include "testincludes.h"
#include "test_utility/localtemporarydirectory.h"
#include "syncpal/syncpal.h"

namespace KDC {

class TestComputeFSOperationWorker : public CppUnit::TestFixture {
        CPPUNIT_TEST_SUITE(TestComputeFSOperationWorker);
        CPPUNIT_TEST(testNoOps);
        CPPUNIT_TEST(testMultipleOps);
        CPPUNIT_TEST(testLnkFileAlreadySynchronized);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override;
        void tearDown() override;

    protected:
        /**
         * No operation should be generated in this test.
         */
        void testNoOps();
        /**
         * Multiple operations of different types should be generated in this test.
         */
        void testMultipleOps();
        /**
         * Specific test for the issue https://infomaniak.atlassian.net/browse/KDESKTOP-893.
         * Sync is looping because a `.lnk` file was already synchronized with an earlier app's version.
         * A Delete FS operation was generated on remote replica but could not be propagated since the file still exists on this 
         * replica.
         * No FS operation should be generated on an excluded file.
         */
        void testLnkFileAlreadySynchronized();

    private:
        std::shared_ptr<SyncPal> _syncPal;
        LocalTemporaryDirectory _localTempDir{"TestSyncPal"};
};

}  // namespace KDC
