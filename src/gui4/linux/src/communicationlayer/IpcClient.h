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

#include <QTcpSocket>
#include <cstdint>

#include <Poco/Dynamic/Struct.h>

#include <string>

namespace KDC {

class IpcClient : public QObject {
        Q_OBJECT

    public:
        explicit IpcClient(QObject *parent = nullptr);
#ifdef QT_DEBUG
        void connectToServer();
#else
        void connectToServer(quint16 port);
#endif
        int32_t sendRequest(RequestNum num, const Poco::DynamicStruct &params = {});

    signals:
        void connected();
        void disconnected();
        void messageReceived(uint8_t type, int32_t id, uint8_t num, const Poco::DynamicStruct &params);

    private slots:
        void onConnected();
        void onDisconnected();
        void onReadyRead();

    private:
        QTcpSocket *_socket;
        std::string _readBuffer;
        QAtomicInteger<int32_t> _nextId{0};

#ifdef QT_DEBUG
        static quint16 readPortFromCommFile();
#endif
        void processBuffer();
        static bool extractNextMessage(std::string &buffer, std::string &outMessage);
};

} // namespace KDC
