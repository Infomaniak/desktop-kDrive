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

//! Signal sent to the client interface to inform it of the state of a file opening requested through a
//! "kdrive://open/..." deeplink, and to close the corresponding pop-up window once the request has been processed.
class SignalFileOpeningStatusJob : public AbstractGuiJob {
    public:
        explicit SignalFileOpeningStatusJob(UniqueId jobId, FileOpeningPhase phase, int8_t percent, const CommString &itemName);

    private:
        ExitInfo deserializeInputParms() override { return ExitCode::Ok; }
        ExitInfo serializeOutputParms() override;
        ExitInfo process() override { return ExitCode::Ok; }

        // Output parameters
        UniqueId _jobId{0};
        FileOpeningPhase _phase{FileOpeningPhase::Finished};
        int8_t _percent{-1};
        CommString _itemName;
};

} // namespace KDC
