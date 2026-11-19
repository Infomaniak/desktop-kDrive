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

#include "fileopeningdownloadjob.h"

#include "appserver.h"

// Input parameters keys
static const auto inParamsDriveDbId = "driveDbId";
static const auto inParamsRemoteNodeId = "remoteNodeId";
static const auto inParamsDestinationFolderPath = "destinationFolderPath";

// Output parameters keys
static const auto outParamsDownloadId = "downloadId";
static const auto outParamsFinalPath = "finalPath";

namespace KDC {

FileOpeningDownloadJob::FileOpeningDownloadJob(std::shared_ptr<CommManager> commManager, int requestId,
                                               const Poco::DynamicStruct &inParams,
                                               std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::FILE_OPENING_DOWNLOAD;
}

ExitInfo FileOpeningDownloadJob::deserializeInputParms() {
    try {
        readParamValue(inParamsDriveDbId, _driveDbId);
        readParamValue(inParamsRemoteNodeId, _remoteNodeId);
        readParamValue(inParamsDestinationFolderPath, _destinationFolderPath);
    } catch (const Poco::Exception &pocoException) {
        LOG_WARN(_logger, "Exception in FileOpeningDownloadJob::readParamValue: error=" << pocoException.message());

        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo FileOpeningDownloadJob::serializeOutputParms() {
    writeParamValue(outParamsDownloadId, _downloadId);
    writeParamValue(outParamsFinalPath, _finalPath);

    return ExitCode::Ok;
}

ExitInfo FileOpeningDownloadJob::process() {
    if (const auto exitInfo = _commManager->appServer().downloadFileOpeningItem(_driveDbId, _remoteNodeId, _destinationFolderPath,
                                                                                _downloadId, _finalPath);
        !exitInfo) {
        LOG_WARN(_logger, "Error in downloadFileOpeningItem for driveDbId=" << _driveDbId << " : " << exitInfo);

        return exitInfo;
    }

    return ExitCode::Ok;
}

} // namespace KDC
