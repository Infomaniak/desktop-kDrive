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
#include <random>

#include "libcommon/log/sentry/handler.h"
#include "libcommon/log/sentry/ptracedescriptor.h"

namespace KDC::sentry {
class AbstractPTrace;
using PTraceUPtr = std::unique_ptr<AbstractPTrace>;
class AbstractPTrace {
    public:
        virtual ~AbstractPTrace() = default;

        // Return the current performance trace id.
        pTraceId id() const { return _pTraceId; }
        virtual void start() { (void) _start(); }
        virtual void stop(const PTraceStatus status = PTraceStatus::Ok) { _stop(status); }
        virtual void restart() { _restart(); }

    protected:
        explicit AbstractPTrace(const PTraceDescriptor &info) : _pTraceInfo(info) {};
        explicit AbstractPTrace(const PTraceDescriptor &info, const int dbId) : _pTraceInfo(info), _syncDbId(dbId) {};

        // Start a new performance trace.
        inline AbstractPTrace &_start() {
            if (!checkCustomSampleRate()) {
                rateLimited = true;
                return *this;
            }
            rateLimited = false;
            _pTraceId = sentry::Handler::instance()->startPTrace(_pTraceInfo, _syncDbId);
            return *this;
        }

        // Stop the performance trace.
        void _stop(const PTraceStatus status = PTraceStatus::Ok) noexcept {
            if (rateLimited) return;
            if (_pTraceId) { // If the performance trace id is set, use it to stop the performance trace (faster).
                Handler::instance()->stopPTrace(_pTraceId, status);
                _pTraceId = 0;
            } else {
                Handler::instance()->stopPTrace(_pTraceInfo, _syncDbId, status);
            }
        }

        // Stop the current performance trace (aborted = false) and start a new one.
        void _restart() {
            _stop();
            (void) _start();
        }

    private:
        AbstractPTrace &operator=(AbstractPTrace &&) = delete;
        pTraceId _pTraceId{0};
        PTraceDescriptor _pTraceInfo;
        int _syncDbId = -1;

        bool rateLimited = false;
        bool checkCustomSampleRate() const {
            if (_pTraceInfo._customSampleRate >= 1.0) return true;
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution dis(0.0, 1.0);
            return dis(gen) < _pTraceInfo._customSampleRate;
        }
};
} // namespace KDC::sentry
