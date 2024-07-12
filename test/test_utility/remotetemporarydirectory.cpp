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

#include "remotetemporarydirectory.h"

#include "libsyncengine/jobs/network/createdirjob.h"
#include "libsyncengine/jobs/network/deletejob.h"
#include "libsyncengine/jobs/network/networkjobsparams.h"

namespace KDC {
RemoteTemporaryDirectory::RemoteTemporaryDirectory(int driveDbId, const NodeId& parentId,
                                                   const std::string& testType /*= "undef"*/)
    : _driveDbId(driveDbId) {
    // Generate directory name
    const std::time_t now = std::time(nullptr);
    const std::tm tm = *std::localtime(&now);
    std::ostringstream woss;
    woss << std::put_time(&tm, "%Y%m%d_%H%M");

    _dirName = "kdrive_" + testType + "_unit_tests_" + woss.str();

    // Create remote test dir
    CreateDirJob job(_driveDbId, parentId, _dirName);
    job.runSynchronously();

    // Extract file ID
    if (job.jsonRes()) {
        Poco::JSON::Object::Ptr dataObj = job.jsonRes()->getObject(dataKey);
        if (dataObj) {
            _dirId = dataObj->get(idKey).toString();
        }
    }
}
RemoteTemporaryDirectory::~RemoteTemporaryDirectory() {
    DeleteJob job(_driveDbId, _dirId, "", "");
    job.setBypassCheck(true);
    job.runSynchronously();
}
}  // namespace KDC