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

#include <libcommon/log/sentry/handler.h>

namespace KDC::Sentry {

// A ScopedPTrace object will start a Performance Trace and stop it when it is destroyed.
struct ScopedPTrace {
        // Start a performance trace, if the trace is not stopped when the object is destroyed, it will be stopped as a
        // failure or success depending on the `manualStopExpected` parameter.
        inline ScopedPTrace(const PTraceName &pTraceName, int syncDbId = -1, bool manualStopExpected = false) :
            _manualStopExpected(manualStopExpected) {
            _pTraceId = Handler::instance()->startPTrace(pTraceName, syncDbId);
        }

        // If the performance trace with the given id is not stopped when the object is destroyed, it will be stopped as a
        // failure.
        inline explicit ScopedPTrace(const pTraceId &transactionId, bool manualStopExpected = false) :
            _pTraceId(transactionId), _manualStopExpected(manualStopExpected) {}
        
        virtual ~ScopedPTrace() { stop(_manualStopExpected); }

        // Return the current performance trace id.
        pTraceId id() const { return _pTraceId; }

        // Stop the current performance trace  (if set) and start a new one with the provided parameters.
        inline void stopAndStart(const PTraceName &pTraceName, int syncDbId = -1, bool manualStopExpected = false) {
            stop(false);
            _pTraceId = Handler::instance()->startPTrace(pTraceName, syncDbId);
            _manualStopExpected = manualStopExpected;
        }

        // Stop the current performance trace  (if set).
        inline void stop(bool aborted = false) noexcept {
            if (_pTraceId) {
                Handler::instance()->stopPTrace(_pTraceId, aborted);
                _pTraceId = 0;
            }
        }

    private:
        pTraceId _pTraceId{0};
        bool _manualStopExpected{false};
        ScopedPTrace &operator=(ScopedPTrace &&) = delete;
};
} // namespace KDC::Sentry
