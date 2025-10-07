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
#include "syncpal/syncpal.h"

using namespace CppUnit;

namespace KDC {

class TestLocalJobs : public CppUnit::TestFixture, public TestBase {
    public:
        CPPUNIT_TEST_SUITE(TestLocalJobs);
        CPPUNIT_TEST(testLocalJobs);
        CPPUNIT_TEST(testLocalDeleteJob);
        CPPUNIT_TEST(testDeleteFilesWithDuplicateNames);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override;
        void tearDown() override;

    protected:
        void testLocalJobs();
        void testLocalDeleteJob();
        void testDeleteFilesWithDuplicateNames();

    private:
        std::shared_ptr<SyncPal> _syncPal = nullptr;
        const std::string _localTempDirName = "test_local_jobs_tmp_dir";
};

} // namespace KDC
