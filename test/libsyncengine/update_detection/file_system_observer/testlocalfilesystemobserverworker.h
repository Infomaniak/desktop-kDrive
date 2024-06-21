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
#include "test_utility/temporarydirectory.h"

#include "db/parmsdb.h"
#include "syncpal/syncpal.h"

#include <log4cplus/logger.h>

using namespace CppUnit;

namespace KDC {

class LocalFileSystemObserverWorker;

class TestLocalFileSystemObserverWorker : public CppUnit::TestFixture {
        CPPUNIT_TEST_SUITE(TestLocalFileSystemObserverWorker);
        CPPUNIT_TEST(testFolderWatcher);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override;
        void tearDown() override;

    protected:
        void testFolderWatcher(void);

    private:
        log4cplus::Logger _logger;
        std::shared_ptr<SyncPal> _syncPal = nullptr;

        static const SyncPath _testFolderPath;
        static const SyncPath _testPicturesFolderName;
        static const uint64_t _nbFileInTestDir;

        TemporaryDirectory _tempDir;
        SyncPath _testRootFolderPath;
};

}  // namespace KDC
