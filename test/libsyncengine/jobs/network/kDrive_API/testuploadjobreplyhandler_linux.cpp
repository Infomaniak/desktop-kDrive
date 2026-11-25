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

#include "testuploadjobreplyhandler_linux.h"

#include "jobs/network/kDrive_API/upload/uploadjobreplyhandler.h"
#include "jobs/network/networkjobsparams.h"
#include "test_utility/localtemporarydirectory.h"

#include <Poco/JSON/Object.h>

#include <fcntl.h>
#include <fstream>
#include <sys/stat.h>

namespace KDC {

namespace {

constexpr SyncTime localCreationTime = 1'600'000'000;
constexpr SyncTime localModificationTime = 1'700'000'000;
constexpr int64_t localModificationTimeNsec = 123'456'789;

SyncPath createFileWithSubSecondModificationTime(const SyncPath &dirPath) {
    const SyncPath filePath = dirPath / "file.txt";
    std::ofstream(filePath) << "content";

    const struct timespec times[2] = {{localModificationTime, localModificationTimeNsec},
                                      {localModificationTime, localModificationTimeNsec}};
    CPPUNIT_ASSERT_EQUAL(0, utimensat(AT_FDCWD, filePath.c_str(), times, AT_SYMLINK_NOFOLLOW));
    return filePath;
}

struct timespec modificationTimespec(const SyncPath &filePath) {
    struct stat sb{};
    CPPUNIT_ASSERT_EQUAL(0, lstat(filePath.c_str(), &sb));
    return sb.st_mtim;
}

Poco::JSON::Object::Ptr makeUploadReply(const SyncTime creationTime, const SyncTime modificationTime) {
    Poco::JSON::Object::Ptr dataObj = new Poco::JSON::Object;
    dataObj->set(idKey, std::string("1"));
    dataObj->set(createdAtKey, creationTime);
    dataObj->set(lastModifiedAtKey, modificationTime);
    dataObj->set(sizeKey, static_cast<int64_t>(7));

    Poco::JSON::Object::Ptr reply = new Poco::JSON::Object;
    reply->set(dataKey, dataObj);
    return reply;
}

} // namespace

void TestUploadJobReplyHandlerLinux::testCreationTimeMismatchKeepsLocalDates() {
    // The backend returns its own creation time for an edited file. Linux cannot apply it, so the local dates must stay
    // untouched, sub-second modification time included.
    const LocalTemporaryDirectory tempDir("TestUploadJobReplyHandlerLinux");
    const SyncPath filePath = createFileWithSubSecondModificationTime(tempDir.path());

    UploadJobReplyHandler handler(filePath, false, localCreationTime, localModificationTime);
    CPPUNIT_ASSERT(handler.extractData(makeUploadReply(localCreationTime + 1000, localModificationTime)));

    // The DB must store the local creation time, otherwise the next sync detects an EDIT and uploads the file again.
    CPPUNIT_ASSERT_EQUAL(localCreationTime, handler.creationTime());
    CPPUNIT_ASSERT_EQUAL(localModificationTime, handler.modificationTime());

    const auto mtime = modificationTimespec(filePath);
    CPPUNIT_ASSERT_EQUAL(localModificationTime, static_cast<SyncTime>(mtime.tv_sec));
    CPPUNIT_ASSERT_EQUAL(localModificationTimeNsec, static_cast<int64_t>(mtime.tv_nsec));
}

void TestUploadJobReplyHandlerLinux::testModificationTimeMismatchAppliesBackendTime() {
    // The backend changed the modification time: it must be applied locally to avoid an endless EDIT loop.
    const LocalTemporaryDirectory tempDir("TestUploadJobReplyHandlerLinux");
    const SyncPath filePath = createFileWithSubSecondModificationTime(tempDir.path());

    const SyncTime backendModificationTime = localModificationTime + 60;
    UploadJobReplyHandler handler(filePath, false, localCreationTime, localModificationTime);
    CPPUNIT_ASSERT(handler.extractData(makeUploadReply(localCreationTime + 1000, backendModificationTime)));

    CPPUNIT_ASSERT_EQUAL(localCreationTime, handler.creationTime());
    CPPUNIT_ASSERT_EQUAL(backendModificationTime, handler.modificationTime());
    CPPUNIT_ASSERT_EQUAL(backendModificationTime, static_cast<SyncTime>(modificationTimespec(filePath).tv_sec));
}

} // namespace KDC
