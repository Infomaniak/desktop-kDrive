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

#include "signalfileopeningstatusjob.h"

// Output parameters keys
static const auto outParamsJobId = "jobId";
static const auto outParamsPhase = "phase";
static const auto outParamsPercent = "percent";
static const auto outParamsItemName = "itemName";
static const auto outParamsItemPath = "itemPath";
static const auto outParamsReceivedBytes = "receivedBytes";
static const auto outParamsTotalBytes = "totalBytes";
static const auto outParamsWebUrl = "webUrl";

namespace KDC {

SignalFileOpeningStatusJob::SignalFileOpeningStatusJob(const UniqueId jobId, const FileOpeningPhase phase, const int8_t percent,
                                                       const CommString &itemName, const CommString &itemPath,
                                                       const int64_t receivedBytes, const int64_t totalBytes) :
    _jobId(jobId),
    _phase(phase),
    _percent(percent),
    _itemName(itemName),
    _itemPath(itemPath),
    _receivedBytes(receivedBytes),
    _totalBytes(totalBytes) {
    _signalNum = SignalNum::FILE_OPENING_STATUS;
}

SignalFileOpeningStatusJob::SignalFileOpeningStatusJob(const UniqueId jobId, const FileOpeningPhase failurePhase,
                                                       const CommString &webUrl) :
    _jobId(jobId),
    _phase(failurePhase),
    _webUrl(webUrl) {
    _signalNum = SignalNum::FILE_OPENING_STATUS;
}

ExitInfo SignalFileOpeningStatusJob::serializeOutputParms() {
    writeParamValue(outParamsJobId, _jobId);
    writeParamValue(outParamsPhase, _phase);
    writeParamValue(outParamsPercent, _percent);
    writeParamValue(outParamsItemName, _itemName);
    writeParamValue(outParamsItemPath, _itemPath);
    writeParamValue(outParamsReceivedBytes, _receivedBytes);
    writeParamValue(outParamsTotalBytes, _totalBytes);
    writeParamValue(outParamsWebUrl, _webUrl);

    return ExitCode::Ok;
}

} // namespace KDC
