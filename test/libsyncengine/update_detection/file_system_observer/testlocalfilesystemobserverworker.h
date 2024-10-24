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

#include <log4cplus/logger.h>

using namespace CppUnit;

namespace KDC {

class LocalFileSystemObserverWorker;

class TestLocalFileSystemObserverWorker : public CppUnit::TestFixture {
        CPPUNIT_TEST_SUITE(TestLocalFileSystemObserverWorker);
        CPPUNIT_TEST(testLFSOWithInitialSnapshot);
        CPPUNIT_TEST(testLFSOWithFiles);
        CPPUNIT_TEST(testLFSOWithDuplicateFileNames);
        CPPUNIT_TEST(testLFSODeleteDir);
        CPPUNIT_TEST(testLFSOWithDirs);
        CPPUNIT_TEST(testLFSOFastMoveDelete);
        CPPUNIT_TEST(testLFSOWithSpecialCases1);
        CPPUNIT_TEST(testLFSOWithSpecialCases2);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override;
        void tearDown() override;

    private:
        log4cplus::Logger _logger;
        std::shared_ptr<SyncPal> _syncPal = nullptr;

        LocalTemporaryDirectory _tempDir;
        SyncPath _rootFolderPath;
        SyncPath _subDirPath;
        std::vector<std::pair<NodeId, SyncPath>> _testFiles;

        void testLFSOWithInitialSnapshot();
        void testLFSOWithFiles();
        void testLFSOWithDuplicateFileNames();
        void testLFSOWithDirs();
        void testLFSODeleteDir();
        void testLFSOFastMoveDelete();
        void testLFSOWithSpecialCases1();
        void testLFSOWithSpecialCases2();
};

} // namespace KDC
