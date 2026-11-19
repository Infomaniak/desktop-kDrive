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

#include "signalfileopeningdownloadstatejob.h"

// Output parameters keys
static const auto outParamsDownloadId = "downloadId";
static const auto outParamsState = "state";
static const auto outParamsPercent = "percent";
static const auto outParamsReceivedBytes = "receivedBytes";
static const auto outParamsTotalBytes = "totalBytes";
static const auto outParamsFinalPath = "finalPath";

namespace KDC {

SignalFileOpeningDownloadStateJob::SignalFileOpeningDownloadStateJob(const UniqueId downloadId,
                                                                     const FileOpeningDownloadState state, const int8_t percent,
                                                                     const int64_t receivedBytes, const int64_t totalBytes,
                                                                     const CommString &finalPath) :
    _downloadId(downloadId),
    _state(state),
    _percent(percent),
    _receivedBytes(receivedBytes),
    _totalBytes(totalBytes),
    _finalPath(finalPath) {
    _signalNum = SignalNum::FILE_OPENING_DOWNLOAD_STATE;
}

ExitInfo SignalFileOpeningDownloadStateJob::serializeOutputParms() {
    writeParamValue(outParamsDownloadId, _downloadId);
    writeParamValue(outParamsState, _state);
    writeParamValue(outParamsPercent, _percent);
    writeParamValue(outParamsReceivedBytes, _receivedBytes);
    writeParamValue(outParamsTotalBytes, _totalBytes);
    writeParamValue(outParamsFinalPath, _finalPath);

    return ExitCode::Ok;
}

} // namespace KDC
