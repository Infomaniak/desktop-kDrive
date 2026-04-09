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

#include "jobs/network/kDrive_API/apitranslator.h"
#include "jobs/network/kDrive_API/getallfilesindirectoryjob.h"
#include "jobs/network/jobexceptions.h"

namespace KDC {
const ApiTranslator::SpecialFolderNames ApiTranslator::v3SpecialFolderNames = {
        {ApiTranslator::SpecialFolder::CommonDocuments, "Common documents"},
        {ApiTranslator::SpecialFolder::Private, "Private"},
        {ApiTranslator::SpecialFolder::Shared, "Shared"}};

std::mutex ApiTranslator::_mutex;


ApiTranslator::RemoteSpecialFoldersCacheMap ApiTranslator::_specialFolderRemoteIdsCache;

RemoteNodeId ApiTranslator::v2RootFolderRemoteId() {
    const auto v2RootFolderRemoteId_ = SyncDb::driveRootNode().nodeIdRemote();

    LOG_IF_FAIL(Log::instance()->getLogger(), v2RootFolderRemoteId_);

    return v2RootFolderRemoteId_.value_or(RemoteNodeId{});
};

bool ApiTranslator::getDriveDbIds(DriveDbIdMap &driveDbIdMap) {
    driveDbIdMap.clear();
    DriveList driveList;
    if (!ParmsDb::instance()->selectAllDrives(driveList)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllDrives.");
        return false;
    }

    for (const auto &drive: driveList) (void) driveDbIdMap.insert({drive.driveId(), drive.dbId()});

    return true;
}

ExitInfo ApiTranslator::getDriveDbId(const DriveId driveId, DriveDbId &driveDbId) {
    driveDbId = 0;

    DriveDbIdMap driveDbIdMap;
    if (!getDriveDbIds(driveDbIdMap)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ApiTranslator::getDriveDbIds.");
        return {ExitCode::DbError, ExitCause::DbAccessError};
    }

    if (const auto it = driveDbIdMap.find(driveId); it != driveDbIdMap.cend()) driveDbId = it->second;

    return ExitCode::Ok;
}

ExitInfo ApiTranslator::updateCache(const DriveId driveId) {
    constexpr auto maxNumberOfItems = 1000;

    const RemoteNodeId remoteFolderId = v2RootFolderRemoteId();
    std::shared_ptr<GetAllFilesInDirectoryJob> fileListJob;

    try {
        fileListJob = std::make_shared<GetAllFilesInDirectoryJob>(driveId, remoteFolderId, TranslationMode::None);
    } catch (const std::bad_alloc &badAllocException) {
        return exception2ExitCode(badAllocException);
    } catch (const JobException &jobException) {
        LOG_WARN(Log::instance()->getLogger(), "Exception thrown by GetAllFilesInDirectoryJob::GetAllFilesInDirectoryJob.")
        return exception2ExitCode(jobException);
    }

    fileListJob->setListingConf({.dirOnly = true, .limit = maxNumberOfItems});
    if (const auto exitInfo = fileListJob->runSynchronously(); !exitInfo) {
        LOG_WARN(Log::instance()->getLogger(), "Error in GetAllFilesInDirectoryJob::runSynchronously: " << exitInfo);
        return exitInfo;
    }

    const auto &nodeInfoList = fileListJob->v3RemoteNodeInfoList();

    const std::scoped_lock lock(_mutex);

    for (const auto specialFolder: {SpecialFolder::CommonDocuments, SpecialFolder::Private, SpecialFolder::Shared}) {
        const auto it = std::find_if(nodeInfoList.cbegin(), nodeInfoList.cend(), [specialFolder](const NodeInfo &nodeInfo) {
            return nodeInfo.name() == v3SpecialFolderNames.at(specialFolder).c_str();
        });
        if (it != nodeInfoList.cend()) _specialFolderRemoteIdsCache[specialFolder][driveId] = it->nodeId().toStdString();
    }

    return ExitCode::Ok;
}

RemoteNodeId ApiTranslator::getValue(const DriveId driveId, const RemoteNodeIdCacheMap &cache) {
    const std::scoped_lock lock(_mutex);
    if (const auto it = cache.find(driveId); it != cache.cend()) return it->second;

    return {};
}

ExitInfo ApiTranslator::getSpecialFolderRemoteId(const DriveId driveId, const SpecialFolder specialFolder,
                                                 RemoteNodeId &folderRemoteId) {
    if (const auto value = getValue(driveId, _specialFolderRemoteIdsCache[specialFolder]); value.empty()) {
        if (const auto exitInfo = updateCache(driveId); !exitInfo) return exitInfo;
    } else {
        folderRemoteId = value;
        return ExitCode::Ok;
    }
    folderRemoteId = getValue(driveId, _specialFolderRemoteIdsCache[specialFolder]);

    return ExitCode::Ok;
}

ExitInfo ApiTranslator::translateV2ToV3(const DriveId driveId, RemoteNodeId &remoteDirectoryId) {
    if (remoteDirectoryId != v2RootFolderRemoteId()) return ExitCode::Ok;

    RemoteNodeId userPrivateFolderRemoteId;
    if (const auto exitInfo = getSpecialFolderRemoteId(driveId, SpecialFolder::Private, userPrivateFolderRemoteId); !exitInfo)
        return exitInfo;
    remoteDirectoryId = userPrivateFolderRemoteId;

    return ExitCode::Ok;
}

void ApiTranslator::translateV3ToV2(SyncPath &remotePath) {
    if (remotePath.empty() || *remotePath.begin() != v3SpecialFolderNames.at(SpecialFolder::Private)) return;

    remotePath = std::filesystem::relative(remotePath, v3SpecialFolderNames.at(SpecialFolder::Private));
    if (remotePath == ".") remotePath = SyncPath{};
}

ExitInfo ApiTranslator::translateV3ToV2(const DriveId driveId, RemoteNodeId &remoteNodeId) {
    RemoteNodeId userPrivateFolderRemoteId;
    if (const auto exitInfo = getSpecialFolderRemoteId(driveId, SpecialFolder::Private, userPrivateFolderRemoteId); !exitInfo)
        return exitInfo;

    if (remoteNodeId != userPrivateFolderRemoteId) return ExitCode::Ok;

    remoteNodeId = v2RootFolderRemoteId();

    return ExitCode::Ok;
}

ExitInfo ApiTranslator::translateV3ToV2(const DriveId driveId, RemoteNodeInfoList &v3RemoteNodeInfoList) {
    RemoteNodeId privateFolderId;
    if (const auto exitInfo = getSpecialFolderRemoteId(driveId, SpecialFolder::Private, privateFolderId); !exitInfo)
        return exitInfo;

    (void) std::erase_if(v3RemoteNodeInfoList, [&privateFolderId](const NodeInfo &nodeInfo) {
        return nodeInfo.nodeId().toStdString() == privateFolderId;
    });

    for (auto &nodeInfo: v3RemoteNodeInfoList) {
        if (nodeInfo.parentNodeId().toStdString() == privateFolderId)
            nodeInfo.setParentNodeId(QString::fromStdString(v2RootFolderRemoteId()));
    }

    return ExitCode::Ok;
}

} // namespace KDC
