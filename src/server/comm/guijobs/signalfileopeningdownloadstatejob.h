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

#include "abstractguijob.h"

namespace KDC {

//! Signal sent to the client interface to inform it of the state of the download of an item triggered from the
//! failure window of a file opening request: the progress while the item is being downloaded, the completion with
//! the path of the downloaded item, or the failure of the download.
class SignalFileOpeningDownloadStateJob : public AbstractGuiJob {
    public:
        explicit SignalFileOpeningDownloadStateJob(UniqueId downloadId, FileOpeningDownloadState state, int8_t percent,
                                                   int64_t receivedBytes, int64_t totalBytes, const CommString &finalPath);

    private:
        ExitInfo deserializeInputParms() override { return ExitCode::Ok; }
        ExitInfo serializeOutputParms() override;
        ExitInfo process() override { return ExitCode::Ok; }

        // Output parameters
        UniqueId _downloadId{0};
        FileOpeningDownloadState _state{FileOpeningDownloadState::Downloading};
        int8_t _percent{-1};
        int64_t _receivedBytes{-1};
        int64_t _totalBytes{-1};
        CommString _finalPath; //!< Path of the downloaded item, set once the download has succeeded.
};

} // namespace KDC
