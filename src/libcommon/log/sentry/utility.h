/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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
#include <libcommon/utility/types.h>
#include "ptraceinfo.h"

namespace KDC::Sentry {
static inline PTraceName SyncSetpToPTraceName(SyncStep step) {
    switch (step) {
        case KDC::SyncStep::UpdateDetection1:
            return PTraceName::UpdateDetection1;
        case KDC::SyncStep::UpdateDetection2:
            return PTraceName::UpdateDetection2;
        case KDC::SyncStep::Reconciliation1:
            return PTraceName::Reconciliation1;
        case KDC::SyncStep::Reconciliation2:
            return PTraceName::Reconciliation2;
        case KDC::SyncStep::Reconciliation3:
            return PTraceName::Reconciliation3;
        case KDC::SyncStep::Reconciliation4:
            return PTraceName::Reconciliation4;
        case KDC::SyncStep::Propagation1:
            return PTraceName::Propagation1;
        case KDC::SyncStep::Propagation2:
            return PTraceName::Propagation2;
        case KDC::SyncStep::Done:
        case KDC::SyncStep::None:
        case KDC::SyncStep::Idle:
            return PTraceName::None;
            break;
        default:
            break;
    }
    return PTraceName();
}
} // namespace KDC
