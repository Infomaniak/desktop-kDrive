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

#include "libcommon/log/sentry/abstractscopedptrace.h"

namespace KDC::sentry {

class AbstractCounterScopedPTrace : public AbstractScopedPTrace {
    public:
        void start() final {
            if (_counter >= _nbOfCyclesPerTrace) {
                AbstractScopedPTrace::stop();
                AbstractScopedPTrace::start();
                _counter = 0;
            } else
                _counter++;
        }

    protected:
        explicit AbstractCounterScopedPTrace(const PTraceDescriptor &info, unsigned int nbOfCyclePerTrace) :
            AbstractScopedPTrace(info, PTraceStatus::Cancelled), _nbOfCyclesPerTrace(nbOfCyclePerTrace) {}

        explicit AbstractCounterScopedPTrace(const PTraceDescriptor &info, unsigned int nbOfCyclePerTrace, int syncDbId) :
            AbstractScopedPTrace(info, PTraceStatus::Cancelled, syncDbId), _nbOfCyclesPerTrace(nbOfCyclePerTrace) {}

    private:
        void stop(PTraceStatus status = PTraceStatus::Ok) final {
            assert(false && "stop() should not be called with CounterScopedPTrace.");
        }
        void restart() final {
            assert(false && "restart() should not be called with CounterScopedPTrace.");
        }
        unsigned int _nbOfCyclesPerTrace = 0; // The number of time start() should be called before stopping the trace.
        unsigned int _counter = 0;
};
} // namespace KDC::sentry
