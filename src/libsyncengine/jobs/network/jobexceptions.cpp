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


#include "jobexceptions.h"

namespace KDC::job_exceptions {

ExitCode exception2ExitCode(const std::exception &exc) {
    if (dynamic_cast<const DbError *>(&exc)) {
        return ExitCode::DbError;
    }
    if (dynamic_cast<const DataError *>(&exc)) {
        return ExitCode::DataError;
    }
    if (dynamic_cast<const TokenError *>(&exc)) {
        return ExitCode::InvalidToken;
    }
    if (dynamic_cast<const InvalidArgumentError *>(&exc)) {
        return ExitCode::LogicError;
    }
    if (dynamic_cast<const std::bad_alloc *>(&exc)) {
        return ExitCode::SystemError;
    }

    return ExitCode::Unknown;
}

} // namespace KDC::job_exceptions
