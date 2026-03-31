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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "signaldispatcher.h"

#include "libcommon/log/customlogstreams.h"

#include <exception>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcSignalDispatcher, "gui.v4.signals", QtInfoMsg)

namespace KDC {

SignalDispatcher::SignalDispatcher(QObject *parent) :
    QObject(parent) {}

void SignalDispatcher::registerHandler(const SignalNum num, SignalHandler handler) {
    _handlers[num].append(std::move(handler));
}

/**
 * Invokes all handlers registered for the given signal number.
 * Unregistered signals are silently ignored (logged at debug level).
 */
void SignalDispatcher::dispatch(const SignalNum num, const Poco::DynamicStruct &params) const {
    const auto it = _handlers.constFind(num);
    if (it == _handlers.constEnd()) {
        qCDebug(lcSignalDispatcher) << "No handler registered for signal num:" << num ;
        return;
    }
    for (const auto& handlers = *it; const auto &handler : handlers) {
        try {
            handler(params);
        } catch (const std::exception &e) {
            qCCritical(lcSignalDispatcher) << "Exception in signal handler for signal:" << num << "-" << e.what();
        } catch (...) {
            qCCritical(lcSignalDispatcher) << "Unknown exception in signal handler for signal:" << num;
        }
    }
}

} // namespace KDC
