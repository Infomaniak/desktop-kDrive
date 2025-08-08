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
#include "utility/types.h"
using namespace CppUnit;

namespace KDC {
class TestLogUploadJob : public CppUnit::TestFixture, public TestBase {
    public:
        CPPUNIT_TEST_SUITE(TestLogUploadJob);
        CPPUNIT_TEST(testLogUploadJobWithOldSessions);
        CPPUNIT_TEST(testLogUploadJobWithoutOldSessions);
        CPPUNIT_TEST(testLogUploadSingleConcurrentJob);
        CPPUNIT_TEST(testLogUploadJobWithoutConnectedUser);
        CPPUNIT_TEST(testLogUploadJobArchiveNotDeletedInCaseOfUploadError);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override;
        void tearDown() override;

    protected:
        void testLogUploadJobWithOldSessions();
        void testLogUploadJobWithoutOldSessions();
        void testLogUploadJobArchiveNotDeletedInCaseOfUploadError();
        void testLogUploadSingleConcurrentJob();
        void testLogUploadJobWithoutConnectedUser();

    private:
        void checkArchiveContent(const SyncPath &archivePath, const std::set<SyncPath> &expectedFiles);
        void getLogDirInfo(std::set<SyncPath> &activeSessionFiles, std::set<SyncPath> &archivedSessionFiles,
                           std::set<SyncPath> &otherFiles);
        void insertUserInDb();

        void createFakeActiveSessionFile(int newNbActiveSessionFiles);
        void createFakeOldSessionFile(int newNbOldSessionFiles);
        void deleteFakeFiles();
};
} // namespace KDC
