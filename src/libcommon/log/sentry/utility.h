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
#include <string>
#include "libcommon/utility/types.h"
#include "libcommon/log/sentry/ptraces.h"

namespace KDC::sentry {
static inline std::unique_ptr<AbstractPTrace> syncStepToPTrace(SyncStep step, int syncDbId) {
    switch (step) {
        case KDC::SyncStep::UpdateDetection1:
            return std::make_unique<sentry::pTraces::basic::UpdateDetection1>(syncDbId);
        case KDC::SyncStep::UpdateDetection2:
            return std::make_unique<sentry::pTraces::basic::UpdateDetection2>(syncDbId);
        case KDC::SyncStep::Reconciliation1:
            return std::make_unique<sentry::pTraces::basic::Reconciliation1>(syncDbId);
        case KDC::SyncStep::Reconciliation2:
            return std::make_unique<sentry::pTraces::basic::Reconciliation2>(syncDbId);
        case KDC::SyncStep::Reconciliation3:
            return std::make_unique<sentry::pTraces::basic::Reconciliation3>(syncDbId);
        case KDC::SyncStep::Reconciliation4:
            return std::make_unique<sentry::pTraces::basic::Reconciliation4>(syncDbId);
        case KDC::SyncStep::Propagation1:
            return std::make_unique<sentry::pTraces::basic::Propagation1>(syncDbId);
        case KDC::SyncStep::Propagation2:
            return std::make_unique<sentry::pTraces::basic::Propagation2>(syncDbId);
        case KDC::SyncStep::Done:
        case KDC::SyncStep::None:
        case KDC::SyncStep::Idle:
            return std::make_unique<sentry::pTraces::None>(syncDbId);
            break;
        default:
            // Log an error when the logger will be available in libcommon.
            break;
    }
    return std::make_unique<sentry::pTraces::None>(syncDbId);
}
} // namespace KDC::sentry
