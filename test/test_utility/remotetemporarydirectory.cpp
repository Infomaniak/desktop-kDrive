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

#include "testincludes.h"
#include "remotetemporarydirectory.h"

#include "../../src/libsyncengine/jobs/network/API_v2/createdirjob.h"
#include "../../src/libsyncengine/jobs/network/API_v2/deletejob.h"
#include "libsyncengine/jobs/network/networkjobsparams.h"
#include "libcommonserver/utility/utility.h"
#include "libcommon/utility/utility.h"

namespace KDC {
RemoteTemporaryDirectory::RemoteTemporaryDirectory(int driveDbId, const NodeId& parentId,
                                                   const std::string& testType /*= "undef"*/) :
    _driveDbId(driveDbId) {
    std::string suffix = CommonUtility::generateRandomStringAlphaNum(5);
    int retry = 5;
    do {
        // Generate directory name
        const std::time_t now = std::time(nullptr);
        const std::tm tm = *std::localtime(&now);
        std::ostringstream woss;
        woss << std::put_time(&tm, "%Y%m%d_%H%M");
        _dirName = Str("kdrive_") + Str2SyncName(testType) + Str("_unit_tests_") + Str2SyncName(woss.str() + "___" + suffix);

        // Create remote test dir
        CreateDirJob job(_driveDbId, parentId, _dirName);
        job.runSynchronously();
        if (job.exitInfo() == ExitInfo(ExitCode::BackError, ExitCause::FileAlreadyExist) && retry > 0) {
            suffix = CommonUtility::generateRandomStringAlphaNum(5);
            retry--;
            continue;
        }

        CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), job.exitInfo());

        // Extract file ID
        CPPUNIT_ASSERT(job.jsonRes());
        Poco::JSON::Object::Ptr dataObj = job.jsonRes()->getObject(dataKey);
        CPPUNIT_ASSERT(dataObj);
        _dirId = dataObj->get(idKey).toString();
        LOGW_INFO(Log::instance()->getLogger(), L"RemoteTemporaryDirectory created: " << Utility::formatSyncName(_dirName)
                                                                                      << L" with ID: "
                                                        << Utility::s2ws(_dirId));
        break;
    } while (true);
}

RemoteTemporaryDirectory::~RemoteTemporaryDirectory() {
    if (_isDeleted) return;

    DeleteJob job(_driveDbId, _dirId, "", "");
    job.setBypassCheck(true);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("~RemoteTemporaryDirectory() failed to delete the directory on remote side.", ExitCode::Ok,
                                 job.runSynchronously());
}
} // namespace KDC
