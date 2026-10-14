/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2026 Infomaniak Network SA
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

#include "testgetmysharedfilesjob.h"

#include "version.h"
#include "jobs/network/kDrive_API/getfilessharedwithmejob.h"
#include "jobs/network/abstracttokennetworkjob.h"
#include "requests/parameterscache.h"

#include "libcommonserver/keychainmanager/keychainmanager.h"
#include "mocks/mockkeychainstorage.h"
#include "keychainmanager/apitoken.h"
#include "libparms/db/parmsdb.h"
#include "mocks/libcommonserver/db/mockdb.h"

#include <Poco/URI.h>

#include <map>
#include <sstream>

using namespace CppUnit;

namespace KDC {

namespace {

// Builds a minimal valid my_shared API JSON response with one directory and one file result.
std::string makeMySharedResponseJson() {
    return R"({"cursor":"c1","has_more":true,"data":[)"
           R"({"id":95755,"name":"Shared directory","type":"dir","parent_id":51006,"last_modified_at":1700000000},)"
           R"({"id":95756,"name":"Shared file.txt","type":"file","parent_id":51006,"size":1700,"last_modified_at":1700000001})"
           R"(]})";
}

} // anonymous namespace

void TestGetFilesSharedWithMeJob::setUp() {
    TestBase::start();
    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ Set Up TestGetFilesSharedWithMeJob");

    initParmsDb();
}

void TestGetFilesSharedWithMeJob::tearDown() {
    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ Tear Down TestGetFilesSharedWithMeJob");

    AbstractTokenNetworkJob::clearCache();
    ParmsDb::instance()->close();
    ParmsDb::reset();
    ParametersCache::reset();
    TestBase::stop();
}

void TestGetFilesSharedWithMeJob::testHandleResponse() {
    GetFilesSharedWithMeJob job(_driveDbId);

    const std::string json = makeMySharedResponseJson();
    std::istringstream is(json);
    const ExitInfo exitInfo = job.handleResponse(is);

    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), exitInfo);
    CPPUNIT_ASSERT_EQUAL(size_t{2}, job.v3RemoteNodeInfoList().size());
    CPPUNIT_ASSERT_EQUAL(std::string("c1"), job.cursor());
    CPPUNIT_ASSERT(job.hasMore());

    // The paths are not requested: they are always empty.
    const NodeInfo &dirInfo = job.v3RemoteNodeInfoList().at(0);
    CPPUNIT_ASSERT_EQUAL(std::string("95755"), dirInfo.nodeId().toStdString());
    CPPUNIT_ASSERT(QString("Shared directory") == dirInfo.name());
    CPPUNIT_ASSERT_EQUAL(std::string("51006"), dirInfo.parentNodeId().toStdString());
    CPPUNIT_ASSERT_EQUAL(qint64{1700000000}, dirInfo.modtime());
    CPPUNIT_ASSERT_EQUAL(qint64{-1}, dirInfo.size()); // No size is extracted for a directory.
    CPPUNIT_ASSERT(dirInfo.path().isEmpty());

    const NodeInfo &fileInfo = job.v3RemoteNodeInfoList().at(1);
    CPPUNIT_ASSERT_EQUAL(std::string("95756"), fileInfo.nodeId().toStdString());
    CPPUNIT_ASSERT(QString("Shared file.txt") == fileInfo.name());
    CPPUNIT_ASSERT_EQUAL(std::string("51006"), fileInfo.parentNodeId().toStdString());
    CPPUNIT_ASSERT_EQUAL(qint64{1700000001}, fileInfo.modtime());
    CPPUNIT_ASSERT_EQUAL(qint64{1700}, fileInfo.size());
    CPPUNIT_ASSERT(fileInfo.path().isEmpty());
}

void TestGetFilesSharedWithMeJob::testHandleResponseTwice() {
    // A retried request handles a second response: its results replace those of the first one instead of piling up.
    GetFilesSharedWithMeJob job(_driveDbId);
    const std::string json = makeMySharedResponseJson();

    for (int32_t attempt = 0; attempt < 2; ++attempt) {
        std::istringstream is(json);
        CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), job.handleResponse(is));
    }

    CPPUNIT_ASSERT_EQUAL(size_t{2}, job.v3RemoteNodeInfoList().size());
}

void TestGetFilesSharedWithMeJob::testHandleResponseMissingDataArray() {
    // A response without a data array is a backend error.
    GetFilesSharedWithMeJob job(_driveDbId);

    const std::string json = R"({"cursor":"c1","has_more":false})";
    std::istringstream is(json);
    const ExitInfo exitInfo = job.handleResponse(is);

    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::BackError, ExitCause::MissingReplyData), exitInfo);
    CPPUNIT_ASSERT_EQUAL(size_t{0}, job.v3RemoteNodeInfoList().size());
}

void TestGetFilesSharedWithMeJob::testSetQueryParameters() {
    const auto queryParameters = [](GetFilesSharedWithMeJob &job) {
        Poco::URI uri("https://api.infomaniak.com/3/drive/1/files/shared_with_me");
        job.setQueryParameters(uri);
        std::map<std::string, std::string> parameters;
        for (const auto &[key, value]: uri.getQueryParameters()) {
            parameters[key] = value;
        }
        return parameters;
    };

    // A first page is requested without cursor.
    GetFilesSharedWithMeJob firstPageJob(_driveDbId);
    auto parameters = queryParameters(firstPageJob);
    CPPUNIT_ASSERT(!parameters.contains("cursor"));
    CPPUNIT_ASSERT_EQUAL(std::string("10"), parameters["limit"]);

    // A next page is requested with the cursor of the previous one.
    GetFilesSharedWithMeJob nextPageJob(_driveDbId, "c1", 50);
    parameters = queryParameters(nextPageJob);
    CPPUNIT_ASSERT_EQUAL(std::string("c1"), parameters["cursor"]);
    CPPUNIT_ASSERT_EQUAL(std::string("50"), parameters["limit"]);
}

void TestGetFilesSharedWithMeJob::testGetSpecificUrl() {
    GetFilesSharedWithMeJob job(_driveDbId);
    CPPUNIT_ASSERT(job.getSpecificUrl().ends_with("/files/my_shared"));
}

} // namespace KDC
