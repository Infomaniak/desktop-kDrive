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

#pragma once

#include "libcommon/comm.h"

#include <QObject>
#include <QString>
#include <QTimer>

#include <Poco/Dynamic/Struct.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>

namespace KDC {

/**
 * Restores the server-assigned order of asynchronous IPC signals before semantic dispatch.
 *
 * The server allocates ids only while the single GUI connection is active, so the first expected id is always
 * firstGuiSignalId. Out-of-order signals are buffered until the missing ids arrive. A gap that persists is skipped rather
 * than fatal: the server can hold a low-priority signal back behind a burst of GUI requests, and the caller resynchronizes
 * its state from `signalsSkipped`. A skipped signal that arrives late is dropped silently. Any other id that was already
 * passed, such as the id 0 the server can send during a burst of signals, is dropped and reported through
 * `staleSignalDropped`, so the caller resynchronizes as well. Negative ids, duplicates in the reorder buffer and buffer
 * overflow remain protocol errors.
 */
class ServerSignalSequencer : public QObject {
        Q_OBJECT

    public:
        explicit ServerSignalSequencer(QObject *parent = nullptr);
        ServerSignalSequencer(std::chrono::milliseconds missingSignalTimeout, size_t maxPendingSignals,
                              QObject *parent = nullptr);

    public slots:
        void enqueue(int32_t signalId, SignalNum num, const Poco::DynamicStruct &params);

    signals:
        void signalReady(SignalNum num, const Poco::DynamicStruct &params);
        void protocolError(const QString &message, const QString &details);
        // The ids from `firstSkippedId` to `lastSkippedId` did not arrive in time and were skipped.
        void signalsSkipped(int32_t firstSkippedId, int32_t lastSkippedId);
        // A signal arrived with an id that was already passed without being skipped, and was dropped.
        void staleSignalDropped(int32_t signalId, int32_t lastForwardedId, SignalNum num);

    private slots:
        void handleMissingSignalTimeout();

    private:
        struct PendingSignal {
                SignalNum num{SignalNum::Unknown};
                Poco::DynamicStruct params;
        };

        static constexpr std::chrono::milliseconds defaultMissingSignalTimeout{10000};
        static constexpr size_t defaultMaxPendingSignals{1024};

        void forwardSignal(int32_t signalId, const PendingSignal &signal);
        void drainContiguousSignals();
        void updateMissingSignalTimer(bool restart);
        void fail(const QString &message, const QString &details);

        std::chrono::milliseconds _missingSignalTimeout;
        size_t _maxPendingSignals;
        QTimer _missingSignalTimer;
        int32_t _lastForwardedId{firstGuiSignalId - 1};
        std::map<int32_t, PendingSignal> _pendingSignals;
        // Skipped ids, so a late arrival is recognized and dropped instead of being reported as stale. Bounded like the
        // reorder buffer: the oldest ids are forgotten first.
        std::set<int32_t> _skippedIds;
        bool _failed{false};
};

} // namespace KDC
