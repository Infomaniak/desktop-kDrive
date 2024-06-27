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

#include "syncpal/syncpal.h"

using namespace CppUnit;

namespace KDC {

class TestRemoteFileSystemObserverWorker : public CppUnit::TestFixture {
        CPPUNIT_TEST_SUITE(TestRemoteFileSystemObserverWorker);
        CPPUNIT_TEST(testGenerateRemoteInitialSnapshot);
        CPPUNIT_TEST(testUpdateSnapshot);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override;
        void tearDown() override;

    protected:
        void testGenerateRemoteInitialSnapshot();
        void testUpdateSnapshot();

    private:
        log4cplus::Logger _logger;
        std::shared_ptr<SyncPal> _syncPal = nullptr;

        int _driveDbId{0};
        NodeId _testFolderId;
        NodeId _testFileId;
};

}  // namespace KDC
