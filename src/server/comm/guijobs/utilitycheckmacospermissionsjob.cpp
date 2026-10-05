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

#include "utilitycheckmacospermissionsjob.h"

#include "libcommon/comm.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/log/log.h"

// Output parameters keys
static const auto outParamsFullDiskAccess = "fullDiskAccess";
static const auto outParamsLiteSyncExtEnabled = "liteSyncExtEnabled";
static const auto outParamsLiteSyncExtFullDiskAccess = "liteSyncExtFullDiskAccess";

namespace KDC {

UtilityCheckMacOsPermissionsJob::UtilityCheckMacOsPermissionsJob(std::shared_ptr<CommManager> commManager, int requestId,
                                                                 const Poco::DynamicStruct &inParams,
                                                                 std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::UTILITY_CHECK_MACOS_PERMISSIONS;
}

ExitInfo UtilityCheckMacOsPermissionsJob::serializeOutputParms() {
    writeParamValue(outParamsFullDiskAccess, _fullDiskAccess);
    writeParamValue(outParamsLiteSyncExtEnabled, _liteSyncExtEnabled);
    writeParamValue(outParamsLiteSyncExtFullDiskAccess, _liteSyncExtFullDiskAccess);

    return ExitCode::Ok;
}

ExitInfo UtilityCheckMacOsPermissionsJob::process() {
    _fullDiskAccess = CommonUtility::isFullDiskAccessAuthOk();
    if (!_fullDiskAccess) {
        // The state of the Lite Sync extension authorization is read from the TCC database, which is itself unreadable without
        // the Full Disk Access authorization.
        LOG_WARN(_logger, "The Server process has not been granted the Full Disk Access authorization");
    }

    _liteSyncExtEnabled = CommonUtility::isLiteSyncExtEnabled();

    std::string liteSyncExtErrorDescr;
    _liteSyncExtFullDiskAccess = CommonUtility::isLiteSyncExtFullDiskAccessAuthOk(liteSyncExtErrorDescr);
    if (!_liteSyncExtFullDiskAccess && !liteSyncExtErrorDescr.empty()) {
        LOG_WARN(_logger, "Error in CommonUtility::isLiteSyncExtFullDiskAccessAuthOk: " << liteSyncExtErrorDescr);
    }

    return ExitCode::Ok;
}

} // namespace KDC
