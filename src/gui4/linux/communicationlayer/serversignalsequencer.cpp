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

#include "serversignalsequencer.h"

#include <QLoggingCategory>

#include <limits>


namespace KDC {

namespace {
Q_LOGGING_CATEGORY(lcServerSignalSequencer, "gui.v4.signalsequencer", QtInfoMsg)
} // namespace

ServerSignalSequencer::ServerSignalSequencer(QObject *const parent) :
    ServerSignalSequencer(defaultMissingSignalTimeout, defaultMaxPendingSignals, parent) {}

ServerSignalSequencer::ServerSignalSequencer(const std::chrono::milliseconds missingSignalTimeout, const size_t maxPendingSignals,
                                             QObject *const parent) :
    QObject(parent),
    _missingSignalTimeout(missingSignalTimeout),
    _maxPendingSignals(maxPendingSignals),
    _missingSignalTimer(this) {
    _missingSignalTimer.setSingleShot(true);
    (void) connect(&_missingSignalTimer, &QTimer::timeout, this, &ServerSignalSequencer::handleMissingSignalTimeout);
}

void ServerSignalSequencer::enqueue(const int32_t signalId, const SignalNum num, const Poco::DynamicStruct &params) {
    if (_failed) {
        return;
    }

    if (signalId < 0) {
        fail(QStringLiteral("Negative server signal id"), QStringLiteral("received id: %1").arg(signalId));
        return;
    }

    if (signalId <= _lastForwardedId) {
        if (_skippedIds.erase(signalId) > 0) {
            qCWarning(lcServerSignalSequencer)
                    << "Late server signal dropped after being skipped | SignalNum:" << static_cast<int32_t>(num)
                    << "/ id:" << signalId;
            return;
        }

        qCWarning(lcServerSignalSequencer) << "Stale server signal dropped | SignalNum:" << static_cast<int32_t>(num)
                                           << "/ id:" << signalId << "/ last forwarded id:" << _lastForwardedId;
        emit staleSignalDropped(signalId, _lastForwardedId, num);
        return;
    }

    if (_lastForwardedId == std::numeric_limits<int32_t>::max()) {
        fail(QStringLiteral("Server signal id overflow"),
             QStringLiteral("last forwarded id: %1 | received id: %2").arg(_lastForwardedId).arg(signalId));
        return;
    }

    const int32_t expectedId = _lastForwardedId + 1;
    if (signalId == expectedId) {
        forwardSignal(signalId, PendingSignal{num, params});
        drainContiguousSignals();
        return;
    }

    if (_pendingSignals.size() >= _maxPendingSignals) {
        if (_pendingSignals.contains(signalId)) {
            fail(QStringLiteral("Duplicate buffered server signal id"),
                 QStringLiteral("received id: %1 | last forwarded id: %2").arg(signalId).arg(_lastForwardedId));
            return;
        }

        fail(QStringLiteral("Server signal reorder buffer overflow"),
             QStringLiteral("expected id: %1 | received id: %2 | buffered signals: %3")
                     .arg(expectedId)
                     .arg(signalId)
                     .arg(static_cast<qulonglong>(_pendingSignals.size())));
        return;
    }

    const bool inserted = _pendingSignals.try_emplace(signalId, num, params).second;
    if (!inserted) {
        fail(QStringLiteral("Duplicate buffered server signal id"),
             QStringLiteral("received id: %1 | last forwarded id: %2").arg(signalId).arg(_lastForwardedId));
        return;
    }

    qCDebug(lcServerSignalSequencer) << "Server signal buffered | id:" << signalId << "/ expected id:" << expectedId
                                     << "/ buffered signals:" << _pendingSignals.size();
    updateMissingSignalTimer(false);
}

void ServerSignalSequencer::forwardSignal(const int32_t signalId, const PendingSignal &signal) {
    _lastForwardedId = signalId;
    emit signalReady(signal.num, signal.params);
}

void ServerSignalSequencer::drainContiguousSignals() {
    while (!_pendingSignals.empty() && _lastForwardedId < std::numeric_limits<int32_t>::max()) {
        const int32_t expectedId = _lastForwardedId + 1;
        const auto pendingSignalsIterator = _pendingSignals.begin();
        if (pendingSignalsIterator->first != expectedId) {
            break;
        }

        const auto pendingSignalNode = _pendingSignals.extract(pendingSignalsIterator);
        qCDebug(lcServerSignalSequencer) << "Server signal propagated in order | SignalNum:"
                                         << static_cast<int32_t>(pendingSignalNode.mapped().num)
                                         << "/ id:" << pendingSignalNode.key();
        forwardSignal(pendingSignalNode.key(), pendingSignalNode.mapped());
    }

    updateMissingSignalTimer(true);
}

void ServerSignalSequencer::updateMissingSignalTimer(const bool restart) {
    if (_pendingSignals.empty()) {
        _missingSignalTimer.stop();
        return;
    }

    if (restart || !_missingSignalTimer.isActive()) {
        _missingSignalTimer.start(_missingSignalTimeout);
    }
}

void ServerSignalSequencer::fail(const QString &message, const QString &details) {
    if (_failed) {
        return;
    }

    _failed = true;
    _missingSignalTimer.stop();
    _pendingSignals.clear();
    qCCritical(lcServerSignalSequencer) << message << "|" << details;
    emit protocolError(message, details);
}

/**
 * Skips the ids missing before the first buffered signal, then delivers the buffered signals that became contiguous. A
 * further gap among them starts a new wait.
 */
void ServerSignalSequencer::handleMissingSignalTimeout() {
    if (_failed || _pendingSignals.empty()) {
        return;
    }

    const int32_t firstSkippedId = _lastForwardedId + 1;
    const int32_t lastSkippedId = _pendingSignals.begin()->first - 1;
    qCWarning(lcServerSignalSequencer) << "Timed out waiting for server signals, skipping them | first skipped id:"
                                       << firstSkippedId << "/ last skipped id:" << lastSkippedId
                                       << "/ buffered signals:" << _pendingSignals.size();

    for (int32_t skippedId = firstSkippedId; skippedId <= lastSkippedId; ++skippedId) {
        (void) _skippedIds.insert(skippedId);
    }

    while (_skippedIds.size() > _maxPendingSignals) {
        (void) _skippedIds.erase(_skippedIds.begin());
    }

    _lastForwardedId = lastSkippedId;
    emit signalsSkipped(firstSkippedId, lastSkippedId);
    drainContiguousSignals();
}

} // namespace KDC
