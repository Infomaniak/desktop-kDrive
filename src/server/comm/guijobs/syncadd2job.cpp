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

#include "syncadd2job.h"
#include "appserver.h"
#include "requests/serverrequests.h"
#include "signalaccountaddedjob.h"
#include "signaldriveaddedjob.h"
#include "signalsyncaddedjob.h"
#include "signalsyncremovedjob.h"
#include "server/comm/guijobmanager.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"

// Input parameters keys
static const auto inParamsDriveDbId = "driveDbId";
static const auto inParamsLocalFolderPath = "localFolderPath";
static const auto inParamsServerFolderPath = "serverFolderPath";
static const auto inParamsServerFolderNodeId = "serverFolderNodeId";
static const auto inParamsLiteSync = "liteSync";
static const auto inParamsBlackList = "blackList";
static const auto inParamsWhiteList = "whiteList";

// Output parameters keys
static const auto outParamsSyncInfo = "syncInfo";

namespace KDC {

SyncAdd2Job::SyncAdd2Job(std::shared_ptr<CommManager> commManager, int requestId, const Poco::DynamicStruct &inParams,
                         std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::SYNC_ADD2;
}

ExitInfo SyncAdd2Job::deserializeInputParms() {
    try {
        readParamValue(inParamsDriveDbId, _driveDbId);

        CommString localFolderPath;
        readParamValue(inParamsLocalFolderPath, localFolderPath);
        _localFolderPath = SyncPath(localFolderPath);

        CommString serverFolderPath;
        readParamValue(inParamsServerFolderPath, serverFolderPath);
        _serverFolderPath = SyncPath(serverFolderPath);

        readParamValue(inParamsServerFolderNodeId, _serverFolderNodeId);
        readParamValue(inParamsLiteSync, _liteSync);
        readParamValues(inParamsBlackList, _blackList);
        readParamValues(inParamsWhiteList, _whiteList);
    } catch (std::exception &e) {
        LOG_WARN(_logger, "Exception in AbstractGuiJob::readParamValue: error=" << e.what());
        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo SyncAdd2Job::serializeOutputParms() {
    // Output parameters serialization
    writeParamValue(outParamsSyncInfo, _syncInfo, info2DynamicVar<SyncInfo>);

    return ExitCode::Ok;
}

ExitInfo SyncAdd2Job::process() {
    // Add sync in DB
#if defined(KD_WINDOWS)
    bool showInNavigationPane = _commManager->appServer().navigationPaneHelper()->showInExplorerNavigationPane();
#else
    bool showInNavigationPane = false;
#endif

    SyncInfo syncInfo;
    if (const auto exitCode = ServerRequests::addSync(_driveDbId, _localFolderPath, _serverFolderPath, _serverFolderNodeId,
                                                      _liteSync, showInNavigationPane, syncInfo);
        exitCode != ExitCode::Ok) {
        LOGW_WARN(_logger, L"Error in Requests::addSync - driveDbId="
                                   << _driveDbId << L" localFolderPath=" << Utility::formatSyncPath(_localFolderPath)
                                   << L" serverFolderPath=" << Utility::formatSyncPath(_serverFolderPath)
                                   << L" serverFolderNodeId=" << Utility::v2ws(_serverFolderNodeId) << L" liteSync=" << _liteSync
                                   << L" showInNavigationPane=" << showInNavigationPane);
        AppServer::addError(Error(ERR_ID, exitCode));
        return exitCode;
    }

    // Check if sync is valid
    Sync sync;
    ServerRequests::syncInfoToSync(syncInfo, sync);
    if (const auto exitInfo = _commManager->appServer().checkIfSyncIsValid(sync); !exitInfo) {
        LOG_WARN(_logger, "Error in checkIfSyncIsValid for syncDbId=" << sync.dbId() << " : " << exitInfo);
        AppServer::addError(Error(sync.dbId(), ERR_ID, exitInfo));
        return exitInfo;
    }

    // Create and start Vfs
    bool start = true;
    if (const auto exitInfo = _commManager->appServer().tryCreateAndStartVfs(sync); !exitInfo) {
        LOG_WARN(_logger, "Error in tryCreateAndStartVfs for syncDbId=" << sync.dbId() << " : " << exitInfo);
        if (!(exitInfo.code() == ExitCode::SystemError && exitInfo.cause() == ExitCause::LiteSyncNotAllowed)) {
            return exitInfo;
        }
        // Continue (ie. Init SyncPal but don't start it)
        start = false;
    }

    // Create and start SyncPal
    NodeSet blackList(std::make_move_iterator(_blackList.begin()), std::make_move_iterator(_blackList.end()));
    NodeSet whiteList(std::make_move_iterator(_whiteList.begin()), std::make_move_iterator(_whiteList.end()));
    if (const auto exitInfo = _commManager->appServer().initSyncPal(sync, blackList, {}, whiteList, start,
                                                                    std::chrono::seconds(0), false, true);
        !exitInfo) {
        LOG_WARN(_logger, "Error in initSyncPal for syncDbId=" << syncInfo.dbId() << " : " << exitInfo);
        AppServer::addError(Error(ERR_ID, exitInfo));

        // Stop SyncPal
        if (const auto exitInfo2 = _commManager->appServer().stopSyncPal(syncInfo.dbId(), false, true, true); !exitInfo2) {
            LOG_WARN(_logger, "Error in stopSyncPal for syncDbId=" << syncInfo.dbId() << " : " << exitInfo2);
        }

        // Stop Vfs
        if (const auto exitInfo2 = _commManager->appServer().stopVfs(syncInfo.dbId(), true); !exitInfo2) {
            LOG_WARN(_logger, "Error in stopVfs for syncDbId=" << syncInfo.dbId() << " : " << exitInfo2);
        }

        SyncPalMap &syncPalMap = _commManager->appServer().syncPalMap();
        LOG_IF_FAIL(!syncPalMap[syncInfo.dbId()] || syncPalMap[syncInfo.dbId()].use_count() == 1)
        syncPalMap.erase(syncInfo.dbId());

        VfsMap &vfsMap = _commManager->appServer().vfsMap();
        LOG_IF_FAIL(!vfsMap[syncInfo.dbId()] || vfsMap[syncInfo.dbId()].use_count() == 1)
        vfsMap.erase(syncInfo.dbId());

        // Delete sync from DB
        if (const ExitInfo exitInfo2 = ServerRequests::deleteSync(syncInfo.dbId()); !exitInfo2) {
            LOG_WARN(_logger, "Error in Requests::deleteSync for syncDbId=" << syncInfo.dbId() << " : " << exitInfo2);
            AppServer::addError(Error(ERR_ID, exitInfo));
        }

        return exitInfo;
    }

    auto signalSyncAddedJob = std::make_shared<SignalSyncAddedJob>(_commManager, syncInfo);
    _commManager->sendGuiSignal(signalSyncAddedJob);

#if defined(KD_MACOS)
    Utility::restartFinderExtension();
#endif

    return ExitCode::Ok;
}

} // namespace KDC
