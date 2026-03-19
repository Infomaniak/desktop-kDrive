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

#include "IpcClient.h"

#include "libcommon/utility/utility.h"

#include <QHostAddress>

#include <Poco/Dynamic/Struct.h>
#include <Poco/JSON/Parser.h>

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
 * Get the port number from the .comm file written by the server's GuiCommServer (new JSON protocol).
 * @return the port read from the .comm file, or 0 if the file cannot be read
 */
quint16 IpcClient::readPortFromCommFile() {
    const std::filesystem::path commPath = CommonUtility::getAppSupportDir() / ".comm";
    std::ifstream commFile(commPath);
    if (!commFile.is_open()) {
        return 0;
    }
    quint16 port = 0;
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

/**
 * @param num  Request number (see RequestNum enum in src/libcommon/comm.h)
 * @param params Request parameters
 * @return the request ID, which can be used to match the response with the request, or -1 if the request could not be sent (e.g. if the socket is not connected or if an error occurs while sending the data)
 */
int32_t IpcClient::sendRequest(RequestNum num, const Poco::DynamicStruct &params) {
    if (_socket->state() != QTcpSocket::ConnectedState) {
        qDebug() << "[IpcClient] Cannot send request, socket not connected";
        return -1;
    }
    const int32_t id = _nextId.fetchAndAddOrdered(1);

    Poco::DynamicStruct msg;
    msg[MSG_TYPE] = 1; // cf. src/server/comm/guijobs/abstractguijob.h GuiJobType enum
    msg[MSG_REQUEST_ID] = id;
    msg[MSG_REQUEST_NUM] = static_cast<uint16_t>(num);
    msg.insert(MSG_REQUEST_PARAMS, params);

    const std::string json = Poco::Dynamic::structToString(msg);
    const auto jsonSize = static_cast<qint64>(json.size());
    const qint64 writtenData = _socket->write(json.data(), jsonSize);

    if (writtenData < 0) {
        qDebug() << "[IpcClient] Failed to send request, error:" << _socket->errorString();
        return -1;
    }

    return id;
}

/**
 * Forwards the socket connected() signal and notifies upper layers.
 */
void IpcClient::onConnected() {
    emit connected();
}

/**
 * Clears the read buffer to avoid processing stale data on next connection,
 * then notifies upper layers.
 */
void IpcClient::onDisconnected() {
    _readBuffer.clear();
    emit disconnected();
}

/**
 * Appends incoming bytes to the read buffer and triggers message extraction with IpcClient::processBuffer.
 */
void IpcClient::onReadyRead() {
    const QByteArray bytes = _socket->readAll();
    _readBuffer.append(bytes.constData(), static_cast<size_t>(bytes.size()));
    processBuffer();
}

/**
 * Process the buffer to extract complete JSON messages and emit a signal for each message received.
 * Each message is expected to be a JSON object with the following structure:
 * {
 *    "type": int, // 1 for requests, 2 for signals
 *    "id": int, // request ID for requests, signal ID for signals
 *    "num": int, // request number for requests, signal number for signals
 *    "params": JSON object
 * }
 *
 */
void IpcClient::processBuffer() {
    Poco::JSON::Parser parser;
    std::string raw;
    while (extractNextMessage(_readBuffer, raw)) {
        try {
            const Poco::Dynamic::Var var = parser.parse(raw);
            const Poco::DynamicStruct msg = *var.extract<Poco::JSON::Object::Ptr>();

            const uint8_t type = msg[MSG_TYPE];
            const int32_t id = msg[MSG_REQUEST_ID];
            const uint8_t num = msg[MSG_REQUEST_NUM];
            const Poco::DynamicStruct params = msg[MSG_REQUEST_PARAMS].extract<Poco::DynamicStruct>();

            emit messageReceived(type, id, num, params);
        } catch (const Poco::Exception &) {
            // TODO add logging when log4cplus is setup in the client
        }
    }
}

/**
 * Extract the first complete JSON message from the buffer.
 * Here, we can count the brackets because every string is base64 encoded so it cannot contain brackets, and there are no escape characters in the JSON messages.
 * @param buffer The buffer containing one or more JSON messages. Each message is expected to be a JSON object starting with '{' and ending with the matching '}'.
 * @param outMessage The extracted JSON message, if a complete message is found at the beginning of the buffer. The message is removed from the buffer.
 * @return true if a complete message was successfully extracted and removed from the buffer, false otherwise (if the buffer is empty, does not start with '{', or does not contain a complete JSON object).
 */
bool IpcClient::extractNextMessage(std::string &buffer, std::string &outMessage) {
    if (buffer.empty() || buffer[0] != '{') {
        return false;
    }

    uint8_t balance = 1;
    for (size_t i = 1; i < buffer.size(); ++i) {
        if (buffer[i] == '{') {
            ++balance;
        } else if (buffer[i] == '}') {
            --balance;
            if (balance == 0) {
                outMessage = buffer.substr(0, i + 1);
                buffer.erase(0, i + 1);
                return true;
            }
        }
    }

    return false;
}

} // namespace KDC
