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
#include "libcommon/utility/types.h"

#include <cstdint>
#include <QHash>
#include <QString>
#include <QTcpSocket>
#include <QTimer>

#include <Poco/Dynamic/Struct.h>

#include <functional>
#include <string>

namespace KDC {

class IpcClient : public QObject {
        Q_OBJECT

    public:
        using ResponseCallback = std::function<void(ExitInfo, const Poco::DynamicStruct &)>;

        explicit IpcClient(QObject *parent = nullptr);
#ifdef QT_DEBUG
        void connectToServer();
#else
        void connectToServer(quint16 port);
#endif
        void sendRequest(RequestNum num, const Poco::DynamicStruct &params = {}, ResponseCallback callback = nullptr);

    signals:
        void connected();
        void disconnected();
        /**
         * Emitted when a server-initiated signal (type:2) has been received and parsed.
         * @param num    Signal number identifying the event (see SignalNum in comm.h)
         * @param params Deserialized JSON parameters associated with the signal
         */
        void serverSignalReceived(SignalNum num, const Poco::DynamicStruct &params);

    private slots:
        void attemptInitialConnection();
        void onConnected();
        void onDisconnected();
        void onErrorOccurred(QAbstractSocket::SocketError socketError);
        void onReadyRead();

    private:
        void handleResponseMessage(const Poco::DynamicStruct &ipcMessage, int32_t id);
        void handleServerSignal(const Poco::DynamicStruct &ipcMessage, int32_t id);

        QTcpSocket *_socket;
        QTimer _initialConnectionRetryTimer;
        std::string _readBuffer;
        int32_t _nextId{0};
        QHash<int32_t, ResponseCallback> _pendingCallbacks;
        bool _hasConnectedOnce{false};
        uint32_t _initialConnectionAttemptCount{0};
        quint16 _configuredPort{0};

#ifdef QT_DEBUG
        static quint16 readPortFromCommFile();
#endif
        void scheduleInitialConnectionRetry(const QString &reason);
        void processBuffer();
        static bool extractNextMessage(std::string &buffer, std::string &outMessage);
};

} // namespace KDC
