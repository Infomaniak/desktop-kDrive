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

#include "syncdeletejob.h"
#include "appserver.h"
#include "requests/serverrequests.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"

// Input parameters keys
static const auto inParamsSyncDbId = "syncDbId";

namespace KDC {

SyncDeleteJob::SyncDeleteJob(std::shared_ptr<CommManager> commManager, int requestId, const Poco::DynamicStruct &inParams,
                             std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::SYNC_DELETE;
}

ExitInfo SyncDeleteJob::deserializeInputParms() {
    try {
        readParamValue(inParamsSyncDbId, _syncDbId);
    } catch (const std::exception &e) {
        LOG_WARN(_logger, "Exception in SyncDeleteJob::readParamValue: error=" << e.what());
        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo SyncDeleteJob::serializeOutputParms() {
    return ExitCode::Ok;
}

ExitInfo SyncDeleteJob::process() {
    _commManager->appServer().stopSyncTask(_syncDbId, SyncPal::DbBehaviorAfterStop::Remove);

    // Delete sync from DB
    _commManager->appServer().deleteSync(_syncDbId);

#if defined(KD_MACOS)
    Utility::restartFinderExtension();
#endif

    return ExitCode::Ok;
}

} // namespace KDC
