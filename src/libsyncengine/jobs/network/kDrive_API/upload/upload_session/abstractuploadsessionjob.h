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


#pragma once

#include "jobs/network/abstracttokennetworkjob.h"

namespace KDC {

class AbstractUploadSessionJob : public AbstractTokenNetworkJob {
    public:
        AbstractUploadSessionJob(UploadSessionType uploadType, DriveDbId driveDbId);
        AbstractUploadSessionJob(UploadSessionType uploadType, DriveDbId driveDbId, const SyncPath &filepath,
                                 const SessionInfo &sessionInfo);
        ~AbstractUploadSessionJob() override = default;

        inline const std::string &sessionToken() const { return _sessionInfo.token; }

    protected:
        SessionInfo _sessionInfo;
        SyncPath _absoluteFilePath;
};

} // namespace KDC
