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

#include "syncpal/syncpal.h"
#include "propagation/executor/executorworker.h"
#include "testincludes.h"


namespace KDC {

class TestExecutorWorker;

class TestExecutorWorker : public CppUnit::TestFixture {
        CPPUNIT_TEST_SUITE(TestExecutorWorker);
        CPPUNIT_TEST(testAffectedUpdateTree);
        CPPUNIT_TEST(testTargetUpdateTree);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override;
        void tearDown() override;

    private:
        void testAffectedUpdateTree();
        void testTargetUpdateTree();

        log4cplus::Logger _logger;
        std::shared_ptr<ExecutorWorker> _testObj = nullptr;
        std::shared_ptr<SyncPal> _syncPal = nullptr;
};

}  // namespace KDC
