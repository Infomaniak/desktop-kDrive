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

#pragma once

#include "logarchiver.h"

namespace KDC {

struct LogArchiverHelper {
        /* Send log to support
         * \param includeArchivedLog If true, all logs will be sent, else only the last session logs will be sent.
         * \param progressCallback The callback to be called with the progress percentage, the function returns false if the user
         * cancels the operation (else true). \param exitCause The exit cause to be filled in case of error. If no error occurred,
         * it will be set to ExitCause::Unknown;
         */
        static ExitCode sendLogToSupport(bool includeArchivedLog,
                                         const std::function<bool(LogUploadState, int)> &progressCallback, ExitCause &exitCause);
        static ExitCode cancelLogToSupport(ExitCause &exitCause);
};

} // namespace KDC
