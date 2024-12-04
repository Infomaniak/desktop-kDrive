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
#include <sentry.h>
#include <list>
#include <assert.h>


namespace KDC::Sentry {
using pTraceId = uint64_t;

// A PerformanceTrace allows to track the duration of the execution of several parts of the code.
// https://docs.sentry.io/product/performance/
struct PerformanceTrace {
        // PerformanceTrace is a structure that represents a sentry_transaction_t or a sentry_span_t that can be sent to
        // Sentry.

        explicit PerformanceTrace(pTraceId pTraceId) : _pTraceId{pTraceId} {
            assert(pTraceId != 0 && "operationId must be different from 0");
        }

        void setTransaction(sentry_transaction_t *tx) { // A transaction allows to track the duration of an operation. It cannot
                                                        // be a child of another transaction.
            assert(!_span);
            _tx = tx;
        }
        void setSpan(sentry_span_t *span) { // A Span works as a transaction, but it needs to have a parent
                                            // (which can be either a transaction or another span).
            assert(!_span);
            _span = span;
        }
        bool isSpan() const { return _span; }
        sentry_span_t *span() const {
            assert(_span);
            return _span;
        }
        sentry_transaction_t *transaction() const {
            assert(_tx);
            return _tx;
        }

        void addChild(pTraceId childId) { _childIds.push_back(childId); }
        void removeChild(pTraceId childId) { _childIds.remove(childId); }
        std::list<pTraceId> &children() { return _childIds; }

        const pTraceId &id() const { return _pTraceId; }

    private:
        pTraceId _pTraceId;
        std::list<pTraceId> _childIds;
        sentry_transaction_t *_tx{nullptr};
        sentry_span_t *_span{nullptr};
};

} // namespace KDC::Sentry
