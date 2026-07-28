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

namespace KDC {

/**
 * Restores the server-assigned order of asynchronous IPC signals before semantic dispatch.
 *
 * The server allocates ids only while the single GUI connection is active, so the first expected id is always
 * firstGuiSignalId. Out-of-order signals are buffered until the missing ids arrive; duplicates, stale ids, persistent gaps
 * and buffer overflow are reported as protocol errors.
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

    private:
        struct PendingSignal {
                SignalNum num{SignalNum::Unknown};
                Poco::DynamicStruct params;
        };

        static constexpr std::chrono::milliseconds defaultMissingSignalTimeout{5000};
        static constexpr size_t defaultMaxPendingSignals{1024};

        void forwardSignal(int32_t signalId, const PendingSignal &signal);
        void drainContiguousSignals();
        void updateMissingSignalTimer(bool restart);
        void fail(const QString &message, const QString &details);
        void handleMissingSignalTimeout();

        std::chrono::milliseconds _missingSignalTimeout;
        size_t _maxPendingSignals;
        QTimer _missingSignalTimer;
        int32_t _lastForwardedId{firstGuiSignalId - 1};
        std::map<int32_t, PendingSignal> _pendingSignals;
        bool _failed{false};
};

} // namespace KDC
