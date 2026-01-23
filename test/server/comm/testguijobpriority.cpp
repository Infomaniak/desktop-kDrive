/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#include "testguijobpriority.h"

#include "comm/guijobmanager.h"
#include "comm/guijobs/blacklistednodesetlistjob.h"
#include "comm/guijobs/nodefoldersizejob.h"
#include "io/iohelper.h"
#include "mocks/libcommonserver/db/mockdb.h"

#include <version.h>

namespace KDC {

void TestGuiJobPriority::setUp() {
    TestBase::start();

    // Create parmsDb
    bool alreadyExists = false;
    std::filesystem::path parmsDbPath = MockDb::makeDbName(alreadyExists);
    (void) IoHelper::deleteItem(parmsDbPath);
    ParmsDb::instance(parmsDbPath, KDRIVE_VERSION_STRING, true, true);
}

void TestGuiJobPriority::tearDown() {
    TestBase::stop();
}

void TestGuiJobPriority::testPriority() {
    GuiJobFactory guiJobFactory;
    JobManagerData jobManagerData;
    auto testChannel = std::make_shared<GuiCommChannel>(Poco::Net::StreamSocket());
    for (auto i = 0; i < 10; i++) {
        auto job = guiJobFactory.make(RequestNum::NODE_FOLDER_SIZE, nullptr, i, {}, testChannel);
        jobManagerData.queue(job, job->jobPriority());
    }
    auto setBlacklistJob = guiJobFactory.make(RequestNum::BLACKLISTED_NODE_SETLIST, nullptr, 10, {}, testChannel);
    jobManagerData.queue(setBlacklistJob, setBlacklistJob->jobPriority());

    auto topJob = jobManagerData._queuedJobs.top().first;
    CPPUNIT_ASSERT_EQUAL(setBlacklistJob->jobId(), topJob->jobId());
}
} // namespace KDC
