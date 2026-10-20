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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "app/errors/blockingerrorresolver.h"

#include <algorithm>

namespace KDC {

std::optional<BlockingErrorKind> blockingErrorKind(const Error &error) {
    if (error.exitCode() == ExitCode::InvalidToken) {
        return BlockingErrorKind::LoggedOut;
    }

    switch (error.exitCause()) {
        case ExitCause::LoginError:
            return BlockingErrorKind::LoggedOut;
        case ExitCause::DriveAsleep:
            return BlockingErrorKind::Asleep;
        case ExitCause::DriveWakingUp:
            return BlockingErrorKind::WakingUp;
        case ExitCause::DriveNotRenew:
            return BlockingErrorKind::NotRenew;
        case ExitCause::DriveMaintenance:
            return BlockingErrorKind::Maintenance;
        case ExitCause::DriveAccessError:
            return BlockingErrorKind::AccessDenied;
        default:
            return std::nullopt;
    }
}

std::optional<BlockingErrorKind> resolveBlockingError(const std::vector<Error> &syncErrorsOldestFirst, const bool userConnected) {
    // The oldest blocking error wins. `SyncContext::latestError` is the most recent error of any kind and must not be used here.
    for (const auto &error: syncErrorsOldestFirst) {
        if (const auto kind = blockingErrorKind(error); kind) {
            return kind;
        }
    }

    // After a restart, the server postpones the synchronizations of a user without token and clears their errors, so no
    // `InvalidToken` error remains. Refreshing the errors removes it as well.
    if (!userConnected) {
        return BlockingErrorKind::LoggedOut;
    }

    return std::nullopt;
}

bool isUpdateRequired(const std::vector<Error> &errors) {
    return std::ranges::any_of(errors, [](const Error &error) { return error.exitCode() == ExitCode::UpdateRequired; });
}

std::string_view toString(const BlockingErrorKind kind) {
    using enum BlockingErrorKind;

    switch (kind) {
        case Asleep:
            return "Asleep";
        case WakingUp:
            return "WakingUp";
        case NotRenew:
            return "NotRenew";
        case Maintenance:
            return "Maintenance";
        case AccessDenied:
            return "AccessDenied";
        case LoggedOut:
            return "LoggedOut";
        case EnumEnd:
            break;
    }
    return "EnumEnd";
}

} // namespace KDC
