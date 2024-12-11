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

#include "libcommon/log/sentry/abstractptrace.h"

namespace KDC::Sentry {

class AbstractScopedPTrace : public AbstractPTrace {
    public:
        ~AbstractScopedPTrace() override { _stop(_autoStopStatus); }

        void start() override { _start(); }

    protected:
        explicit AbstractScopedPTrace(const PTraceDescriptor &info, PTraceStatus autoStopStatus) :
            AbstractPTrace(info), _autoStopStatus(autoStopStatus) {
            start();
        }
        explicit AbstractScopedPTrace(const PTraceDescriptor &info, PTraceStatus autoStopStatus, int syncDbId) :
            AbstractPTrace(info, syncDbId), _autoStopStatus(autoStopStatus) {
            start();
        }

    private:
        PTraceStatus _autoStopStatus = PTraceStatus::Ok; // The status to use when the object is stopped due to its destruction.
};
} // namespace KDC::Sentry
