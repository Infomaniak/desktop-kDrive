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
#include "server/updater/updatechecker.h"
#include "libsyncengine/jobs/network/mockgetappversionjob.h"

namespace KDC {

class MockUpdateChecker : public UpdateChecker {
    public:
        using UpdateChecker::UpdateChecker;
        void setUpdateShouldBeAvailable(const bool val) { _updateShouldBeAvailable = val; }
        void setAllVersionInfo(const AllVersionsInfo &versionInfo) { _versionsInfo = versionInfo; }

    private:
        ExitCode generateGetAppVersionJob(std::shared_ptr<AbstractNetworkJob> &job) override {
            static const std::string appUid = "1234567890";
            job = std::make_shared<MockGetAppVersionJob>(CommonUtility::platform(), appUid, _updateShouldBeAvailable);
            return ExitCode::Ok;
        }

        bool _updateShouldBeAvailable{false};
};
} // namespace KDC
