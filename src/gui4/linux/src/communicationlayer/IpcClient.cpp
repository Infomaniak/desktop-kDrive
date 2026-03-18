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

#include "IpcClient.h"

#include "libcommon/utility/utility.h"

#include <QHostAddress>

#include <filesystem>
#include <fstream>

namespace KDC {

IpcClient::IpcClient(QObject *parent) :
    QObject(parent),
    _socket(new QTcpSocket(this)) {
    connect(_socket, &QTcpSocket::connected, this, &IpcClient::onConnected);
    connect(_socket, &QTcpSocket::disconnected, this, &IpcClient::onDisconnected);
    connect(_socket, &QTcpSocket::readyRead, this, &IpcClient::onReadyRead);
}


#ifdef QT_DEBUG
/**
 * In debug mode, the port is read from the .comm file created by the server
 */
void IpcClient::connectToServer() {
    const quint16 port = readPortFromCommFile();
    if (port == 0) {
        emit disconnected();
        return;
    }
    _socket->connectToHost(QHostAddress::LocalHost, port);
}

/**
 * Get the port number from the .comm file located in the application support directory.
 * @return the port read from the .comm file, or 0 if the file cannot be read
 */
quint16 IpcClient::readPortFromCommFile() {
    const std::filesystem::path commPath = CommonUtility::getAppSupportDir() / ".comm";
    std::ifstream commFile(commPath);
    if (!commFile.is_open()) {
        return 0;
    }
    unsigned short port = 0;
    commFile >> port;
    return port;
}

#else
/**
 * In release mode, the port is passed as a command-line argument by the server when launching the client.
 */
void IpcClient::connectToServer(quint16 port) {
    if (port == 0) {
        emit disconnected();
        return;
    }
    _socket->connectToHost(QHostAddress::LocalHost, port);
}
#endif

} // namespace KDC
