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
#include "update_detection/file_system_observer/folderwatcher_linux.h"

namespace KDC {

class TestFolderWatcherLinux final : public CppUnit::TestFixture, public TestBase {
        CPPUNIT_TEST_SUITE(TestFolderWatcherLinux);
        CPPUNIT_TEST(testMakeSyncPath);
        CPPUNIT_TEST(testRemoveFoldersBelow);
        CPPUNIT_TEST(testInotifyRegisterPath);
        CPPUNIT_TEST(testFindSubFolders);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp(void) final { TestBase::start(); }
        void tearDown(void) final { TestBase::stop(); }

    private:
        void testMakeSyncPath();
        void testAddFolderRecursive();
        void testRemoveFoldersBelow();
        void testInotifyRegisterPath();
        void testFindSubFolders();
};

} // namespace KDC
