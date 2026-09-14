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

#include "testabstractnetworkjob.h"

#include "jobs/network/abstractnetworkjob.h"
#include "libparms/db/parmsdb.h"
#include "mocks/libcommonserver/db/mockdb.h"
#include "version.h"

#include <Poco/Net/HTTPRequest.h>

namespace KDC {

namespace {

class TestJob final : public AbstractNetworkJob {
    public:
        using AbstractNetworkJob::setHeaders; // Expose the protected method to the test

    protected:
        ExitInfo handleResponse(std::istream &) override { return ExitCode::Ok; }
        ExitInfo handleError(const std::string &, const Poco::URI &) override { return ExitCode::Ok; }
        std::string getSpecificUrl() override { return "/"; }
        std::string getUrl() override { return "/"; }
};

} // namespace

void TestAbstractNetworkJob::setUp() {
    TestBase::start();
    (void) ParmsDb::instance(_localTempDir.path() / MockDb::makeDbMockFileName(), KDRIVE_VERSION_STRING, true, true);
}

void TestAbstractNetworkJob::tearDown() {
    ParmsDb::reset();
    TestBase::stop();
}

void TestAbstractNetworkJob::testAppUIDIsSentInClientAppIdHeader() {
    const auto appUID = ParmsDb::appUID();
    CPPUNIT_ASSERT(!appUID.empty());

    TestJob job;
    Poco::Net::HTTPRequest req(Poco::Net::HTTPRequest::HTTP_GET, "/");
    job.setHeaders(req);

    CPPUNIT_ASSERT_EQUAL(appUID, req.get("ik-client-app-id", ""));
}

void TestAbstractNetworkJob::testExplicitContextIsPassedThroughRequestContext() {
    const std::string customContext("custom context");

    TestJob job;
    job.setContext(customContext);
    Poco::Net::HTTPRequest req(Poco::Net::HTTPRequest::HTTP_GET, "/");
    job.setHeaders(req);

    CPPUNIT_ASSERT_EQUAL(customContext, req.get("ik-client-context", ""));
    CPPUNIT_ASSERT_EQUAL(ParmsDb::appUID(), req.get("ik-client-app-id", ""));
}

} // namespace KDC
