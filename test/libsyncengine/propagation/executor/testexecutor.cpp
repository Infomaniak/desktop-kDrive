/*
 *
 *  * Infomaniak kDrive - Desktop
 *  * Copyright (C) 2023-2024 Infomaniak Network SA
 *  *
 *  * This program is free software: you can redistribute it and/or modify
 *  * it under the terms of the GNU General Public License as published by
 *  * the Free Software Foundation, either version 3 of the License, or
 *  * (at your option) any later version.
 *  *
 *  * This program is distributed in the hope that it will be useful,
 *  * but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  * GNU General Public License for more details.
 *  *
 *  * You should have received a copy of the GNU General Public License
 *  * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "testexecutor.h"
#include "propagation/executor/executorworker.h"
#include "libcommonserver/io/testio.h"
#include "server/vfs/mac/litesyncextconnector.h"
#include "io/filestat.h"

namespace KDC {

void TestExecutor::setUp() {
    _testObj = std::make_shared<Executor>(nullptr, "", "");
}

void TestExecutor::tearDown() {}

void TestExecutor::testCheckLiteSyncInfoForCreate() {
    // A hydrated placeholder
    {
        // Create temp directory and file.
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "test_file.txt";
        {
            std::ofstream ofs(path);
            ofs << "abc";
            ofs.close();
        }
        // Convert file to placeholder
        const auto extConnector = LiteSyncExtConnector::instance(Log::instance()->getLogger(), ExecuteCommand());
        extConnector->vfsConvertToPlaceHolder(Path2QStr(path), true);

        FileStat fileStat;
        IoError ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->getFileStat(path, &fileStat, ioError));
        CPPUNIT_ASSERT(!fileStat.isHidden);
        CPPUNIT_ASSERT(fileStat.nodeType == NodeTypeFile);
        CPPUNIT_ASSERT(!fileStat.isEmptyOnDisk);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // A dehydrated placeholder
    {
        // Create temp directory and file.
        const TemporaryDirectory temporaryDirectory;
        // Create placeholder
        const auto extConnector = LiteSyncExtConnector::instance(Log::instance()->getLogger(), ExecuteCommand());
        const auto timeNow =
            std::chrono::duration_cast<std::chrono::hours>(std::chrono::system_clock::now().time_since_epoch()).count();
        struct stat fileInfo;
        fileInfo.st_size = 123;
        fileInfo.st_mtimespec = {timeNow, 0};
        fileInfo.st_atimespec = {timeNow, 0};
        fileInfo.st_birthtimespec = {timeNow, 0};
        fileInfo.st_mode = S_IFREG;
        SyncName fileName = "test_file.txt";
        extConnector->vfsCreatePlaceHolder(SyncName2QStr(fileName), Path2QStr(temporaryDirectory.path), &fileInfo);

        FileStat fileStat;
        IoError ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->getFileStat(temporaryDirectory.path / fileName, &fileStat, ioError));
        CPPUNIT_ASSERT(!fileStat.isHidden);
        CPPUNIT_ASSERT(fileStat.nodeType == NodeTypeFile);
        CPPUNIT_ASSERT(fileStat.isEmptyOnDisk);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }
}

}  // namespace KDC