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
#include "libcommon/utility/debughelpers.h"
#include "libcommon/utility/utility.h"
#include "libcommon/utility/cstypes.h"

#include <QHostAddress>
#include <QLoggingCategory>

#include <Poco/Dynamic/Struct.h>
#include <Poco/JSON/Parser.h>

#include <filesystem>
#include <exception>
#include <fstream>
#include <utility>

Q_LOGGING_CATEGORY(lcIpcClient, "gui.v4.ipc", QtInfoMsg)

// Response-only fields (type=QUERY) - defined server-side in abstractguijob.cpp, not in comm.h
static constexpr auto MSG_RESPONSE_CODE = "code";
static constexpr auto MSG_RESPONSE_CAUSE = "cause";

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
 * @param callback Optional callback invoked with the server response. If null, the response is silently discarded.
 * @return the request ID, which can be used to match the response with the request.
 * @note Any communication error (socket not connected, serialization failure, partial write) is considered fatal: the client calls exit(EXIT_FAILURE).
 */
int32_t IpcClient::sendRequest(RequestNum num, const Poco::DynamicStruct &params, ResponseCallback callback) {
    if (_socket->state() != QAbstractSocket::ConnectedState) {
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

    if (callback) {
        _pendingCallbacks[id] = std::move(callback);
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
 * Process the buffer to extract complete JSON messages and route each one.
 * - type: GuiJobType::Query (1)  -> response to a pending request: invoke its stored callback and remove it.
 * - type: GuiJobType::Signal (2) -> server-initiated signal: emit serverSignalReceived().
 *
 * Each message is expected to be a JSON object:
 * { "type": int, "id": int, "num": int, "params": object }
 *
 * @note Any parsing or deserialization error is considered fatal: the client calls exit(EXIT_FAILURE).
 */
void IpcClient::processBuffer() {
    Poco::JSON::Parser parser;
    std::string raw;
    while (extractNextMessage(_readBuffer, raw)) {
        qCDebug(lcIpcClient) << "Received message:" << QString::fromStdString(raw);
        try {
            parser.reset();
            const Poco::Dynamic::Var var = parser.parse(raw);
            const auto &objPtr = var.extract<Poco::JSON::Object::Ptr>();
            if (!objPtr) {
                qCCritical(lcIpcClient) << "Failed to parse the JSON message: \n'" << raw << "'\n";
                exit(EXIT_FAILURE);
            }
            const Poco::DynamicStruct msg = *objPtr;

            if (!msg.contains(MSG_TYPE) || !msg.contains(MSG_REQUEST_ID) || !msg.contains(MSG_REQUEST_NUM) ||
                !msg.contains(MSG_REQUEST_PARAMS)) {
                qCCritical(lcIpcClient) << "Received malformed message, missing required fields";
                exit(EXIT_FAILURE);
            }

            const auto type = static_cast<GuiJobType>(msg[MSG_TYPE].convert<int>());
            const int32_t id = msg[MSG_REQUEST_ID];
            const Poco::DynamicStruct params = msg[MSG_REQUEST_PARAMS].extract<Poco::DynamicStruct>();

            switch (type) {
                case GuiJobType::Query: {
                    if (!msg.contains(MSG_RESPONSE_CODE) || !msg.contains(MSG_RESPONSE_CAUSE)) {
                        qCCritical(lcIpcClient) << "Response missing code/cause fields for id:" << id;
                        exit(EXIT_FAILURE);
                    }
                    const auto exitCode = static_cast<ExitCode>(msg[MSG_RESPONSE_CODE].convert<int>());
                    const auto exitCause = static_cast<ExitCause>(msg[MSG_RESPONSE_CAUSE].convert<int>());
                    const auto num = static_cast<RequestNum>(msg[MSG_REQUEST_NUM].convert<int>());
                    qCDebug(lcIpcClient) << "Reply received | RequestNum:" << num << "/ id:" << id;
                    auto it = _pendingCallbacks.find(id);
                    if (it != _pendingCallbacks.end()) {
                        auto callback = std::move(it.value());
                        _pendingCallbacks.erase(it);

                        try {
                            callback(ExitInfo(exitCode, exitCause), params);
                        } catch (const std::exception &e) {
                            qCCritical(lcIpcClient) << "Exception in response callback for request id:" << id << "(RequestNum:" << num << ") -" << e.what();
                        } catch (...) {
                            qCCritical(lcIpcClient) << "Unknown exception in response callback for request id:" << id << "(RequestNum:" << num << ")";
                        }
                    } else {
                        qCWarning(lcIpcClient) << "Received response for unknown request id:" << id;
                    }
                    break;
                }
                case GuiJobType::Signal: {
                    const auto num = static_cast<SignalNum>(msg[MSG_REQUEST_NUM].convert<int>());
                    qCDebug(lcIpcClient) << "Signal received | SignalNum:" << num << "/ id:" << id;
                    emit serverSignalReceived(num, params);
                    break;
                }
                default:
                    qCCritical(lcIpcClient) << "Received message with unknown type:" << type;
                    exit(EXIT_FAILURE);
            }
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
