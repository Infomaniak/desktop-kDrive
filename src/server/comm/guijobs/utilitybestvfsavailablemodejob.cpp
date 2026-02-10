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

#include "utilitybestvfsavailablemodejob.h"
#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"

// Input parameters keys
static const auto inParamsPath = "path";

// Output parameters keys
static const auto outParamsBestMode = "bestMode";

namespace KDC {

UtilityBestVfsAvailableModeJob::UtilityBestVfsAvailableModeJob(std::shared_ptr<CommManager> commManager, int requestId,
                                                               const Poco::DynamicStruct &inParams,
                                                               std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::UTILITY_BESTVFSAVAILABLEMODE;
}

ExitInfo UtilityBestVfsAvailableModeJob::deserializeInputParms() {
    constexpr auto logMessage = "Exception in UtilityBestVfsAvailableModeJob::readParamValue: error=";
    std::string pathStr;
    try {
        readParamValue(inParamsPath, pathStr);
    } catch (const Poco::Exception &pocoException) {
        LOG_WARN(_logger, logMessage << pocoException.message());

        return ExitCode::LogicError;
    }
    _path = SyncPath(pathStr);

    return ExitCode::Ok;
}

ExitInfo UtilityBestVfsAvailableModeJob::serializeOutputParms() {
    writeParamValue(outParamsBestMode, _bestMode);
    return ExitCode::Ok;
}

ExitInfo UtilityBestVfsAvailableModeJob::process() {
    const VirtualFileMode mode = KDC::bestAvailableVfsMode();
    if (mode == VirtualFileMode::Off) {
        _bestMode = VirtualFileMode::Off;
        return ExitCode::Ok;
    }

    if (_path.empty() || !_path.is_absolute()) {
        LOGW_WARN(_logger, L"The provided path is empty or not absolute: " << Utility::formatSyncPath(_path));
        _bestMode = VirtualFileMode::Off;
        return ExitCode::LogicError;
    }

    // Check if VFS can be used on the given path
#ifdef KD_WINDOWS
    if (!CommonUtility::isNTFS(_path)) {
        LOGW_DEBUG(_logger, L"The file system is not NTFS: " << Utility::formatSyncPath(_path) << L". VFS cannot be used.");
        _bestMode = VirtualFileMode::Off;
        return ExitCode::Ok;
    }

    if (_path == _path.root_path()) {
        LOGW_DEBUG(_logger, L"The path is the root of a disk: " << Utility::formatSyncPath(_path) << L". VFS cannot be used.");
        _bestMode = VirtualFileMode::Off;
        return ExitCode::Ok;
    }
#endif // KD_WINDOWS

#ifdef KD_MACOS
    if (!CommonUtility::isAPFS(_path)) {
        LOGW_DEBUG(_logger, L"The file system is not APFS: " << Utility::formatSyncPath(_path) << L". VFS cannot be used.");
        _bestMode = VirtualFileMode::Off;
        return ExitCode::Ok;
    }
#endif // KD_MACOS

    _bestMode = mode;
    return ExitCode::Ok;
}

} // namespace KDC
