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

#include "ipcclient.h"

#include "libcommon/utility/utility.h"
#include "libcommon/utility/cstypes.h"

#include <QHostAddress>
#include <QLoggingCategory>

#include <Poco/Dynamic/Struct.h>
#include <Poco/JSON/Parser.h>

#include <filesystem>
#include <fstream>
#include <utility>

Q_LOGGING_CATEGORY(lcIpcClient, "gui.v4.ipc", QtInfoMsg)

namespace KDC {

IpcClient::IpcClient(QObject *parent) :
    QObject(parent),
    _socket(new QTcpSocket(this)) {
    connect(_socket, &QTcpSocket::connected, this, &IpcClient::onConnected);
    connect(_socket, &QTcpSocket::readyRead, this, &IpcClient::onReadyRead);
    connect(_socket, &QTcpSocket::disconnected, this, [this] {
        emit disconnected();
        exit(EXIT_FAILURE);
    });
    connect(_socket, &QTcpSocket::errorOccurred, this, [this](const QAbstractSocket::SocketError socketError) {
        qCCritical(lcIpcClient) << "Socket error:" << socketError << "-" << _socket->errorString();
        qCCritical(lcIpcClient) << "This error is considered fatal, exiting.";
        exit(EXIT_FAILURE);
    });
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
        qCWarning(lcIpcClient) << "Failed to open .comm file at" << QString::fromStdString(commPath.string());
        return 0;
    }
    quint16 port = 0;
    commFile >> port;
    qCDebug(lcIpcClient) << "Port read from .comm file:" << port;
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
    qCDebug(lcIpcClient) << "Connecting socket to server on port" << port;
    _socket->connectToHost(QHostAddress::LocalHost, port);
}
#endif

/**
 * @param num  Request number (see RequestNum enum in src/libcommon/comm.h)
 * @param params Request parameters
 * @return the request ID, which can be used to match the response with the request.
 * @note Any communication error (socket not connected, serialization failure, partial write) is considered fatal: the client calls exit(EXIT_FAILURE).
 */
int32_t IpcClient::sendRequest(RequestNum num, const Poco::DynamicStruct &params) {
    if (_socket->state() != QTcpSocket::ConnectedState) {
        qCCritical(lcIpcClient) << "Cannot send request, socket not in ConnectedState mode (state: " << _socket->state() << ")"; // See qabstractsocket.h#SocketState
        exit(EXIT_FAILURE);
    }
    const int32_t id = _nextId++;

    Poco::DynamicStruct msg;
    msg[MSG_TYPE] = static_cast<std::underlying_type_t<GuiJobType>>(GuiJobType::Query);
    msg[MSG_REQUEST_ID] = id;
    msg[MSG_REQUEST_NUM] = static_cast<std::underlying_type_t<RequestNum>>(num); // Sonar cpp:S7035 - approximatively equivclent to static_cast<uint16_t>(num);

    if (const bool insertResult = msg.insert(MSG_REQUEST_PARAMS, params).second; !insertResult) {
        qCCritical(lcIpcClient) << "Failed to insert request parameters into message";
        exit(EXIT_FAILURE);
    }

    const std::string json = Poco::Dynamic::structToString(msg);
    const auto jsonSize = static_cast<qint64>(json.size());

    if (const qint64 writtenData = _socket->write(json.data(), jsonSize); writtenData != jsonSize) {
        if (writtenData < 0) {
            qCCritical(lcIpcClient) << "Failed to send request, error:" << _socket->errorString();
        } else {
            // Very unlikely: the kernel buffer is filled by an internal write to the fd; if the server is not reading in time, another issue is probably occurring on its side.
            qCCritical(lcIpcClient) << "Partial write detected, expected:" << jsonSize << "written:" << writtenData;
        }
        exit(EXIT_FAILURE);
    }

    return id;
}

/** Forwards the socket connected() signal and notifies upper layers. */
void IpcClient::onConnected() {
    emit connected();
}

/** Appends incoming bytes to the read buffer and triggers message extraction with IpcClient::processBuffer. */
void IpcClient::onReadyRead() {
    const QByteArray bytes = _socket->readAll();
    (void) _readBuffer.append(bytes.constData(), static_cast<size_t>(bytes.size()));
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
 * @note Any parsing or deserialization error is considered fatal: the client calls exit(EXIT_FAILURE).
 */
void IpcClient::processBuffer() {
    Poco::JSON::Parser parser;
    std::string raw;
    while (extractNextMessage(_readBuffer, raw)) {
        try {
            parser.reset();
            const Poco::Dynamic::Var var = parser.parse(raw);
            const auto& objPtr = var.extract<Poco::JSON::Object::Ptr>();
            if (!objPtr) {
                qCCritical(lcIpcClient) << "Failed to parse the JSON message: \n'" << raw << "'\n";
                exit(EXIT_FAILURE);
            }
            const Poco::DynamicStruct msg = *objPtr;

            if (!msg.contains(MSG_TYPE) || !msg.contains(MSG_REQUEST_ID) || !msg.contains(MSG_REQUEST_NUM) || !msg.contains(MSG_REQUEST_PARAMS)) {
                qCCritical(lcIpcClient) << "Received malformed message, missing required fields";
                exit(EXIT_FAILURE);
            }

            const auto type = static_cast<GuiJobType>(msg[MSG_TYPE].convert<int>());
            const int32_t id = msg[MSG_REQUEST_ID];
            const uint8_t num = msg[MSG_REQUEST_NUM];
            const Poco::DynamicStruct params = msg[MSG_REQUEST_PARAMS].extract<Poco::DynamicStruct>();

            emit messageReceived(type, id, num, params);
        } catch (const Poco::Exception &e) {
            qCCritical(lcIpcClient) << "Exception while processing message:" << e.what();
            exit(EXIT_FAILURE);
        }
    }
}

/**
 * Extract the first complete JSON message from the buffer.
 * Here, we can count the brackets because every string is base64 encoded so it cannot contain brackets, and there are no escape characters in the JSON messages.
 * @param buffer The buffer containing one or more JSON messages. Each message is expected to be a JSON object starting with '{' and ending with the matching '}'.
 * @param outMessage The extracted JSON message, if a complete message is found at the beginning of the buffer. The message is removed from the buffer.
 * @return true if a complete message was successfully extracted and removed from the buffer, false if the buffer is empty or does not yet contain a complete JSON object.
 * @note Malformed input (buffer not starting with '{', or too many nested objects) is considered fatal: the client calls exit(EXIT_FAILURE).
 */
bool IpcClient::extractNextMessage(std::string &buffer, std::string &outMessage) {
    if (buffer.empty()) {
        return false;
    }
    
    if (buffer[0] != '{') {
        qCCritical(lcIpcClient) << "Invalid message format: buffer does not start with '{'";
        exit(EXIT_FAILURE);
    }

    uint8_t balance = 1;
    for (size_t i = 1; i < buffer.size(); ++i) {
        if (buffer[i] == '{') {
            if (balance == std::numeric_limits<decltype(balance)>::max()) { // Avoid overflow of balance variable, which would cause incorrect parsing and potential security issues
                qCCritical(lcIpcClient) << "Invalid message format: too many nested objects";
                exit(EXIT_FAILURE);
            }
            ++balance;
        } else if (buffer[i] == '}') {
            --balance;
            if (balance == 0) {
                outMessage.assign(buffer, 0, i + 1);
                buffer.erase(0, i + 1);
                return true;
            }
        }
    }

    return false;
}

} // namespace KDC
