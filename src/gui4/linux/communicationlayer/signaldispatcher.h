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

#pragma once

#include "libcommon/comm.h"

#include <QHash>
#include <QObject>

#include <Poco/Dynamic/Struct.h>

#include <functional>

namespace KDC {

/**
 * Dispatches server-initiated signals (type:2 messages) to registered handlers.
 *
 * Multiple handlers can be registered for the same SignalNum.
 * All dispatch calls happen on the Qt main thread (called from IpcClient::onReadyRead).
 */
class SignalDispatcher : public QObject {
        Q_OBJECT

    public:
        using Handler = std::function<void(Poco::DynamicStruct)>;

        explicit SignalDispatcher(QObject *parent = nullptr);

        /**
         * Register a handler for a given server signal.
         * Multiple handlers for the same SignalNum are all called in registration order.
         */
        void registerHandler(SignalNum num, Handler handler);

    public slots:
        /**
         * Called when a server signal is received. Invokes all handlers registered for @p num.
         */
        void dispatch(SignalNum num, Poco::DynamicStruct params);

    private:
        QHash<SignalNum, QList<Handler>> _handlers;
};

} // namespace KDC
