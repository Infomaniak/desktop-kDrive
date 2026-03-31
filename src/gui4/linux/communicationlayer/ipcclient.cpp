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
#include "libcommon/utility/types.h"

#include <QHostAddress>
#include <QLoggingCategory>

#include <Poco/Dynamic/Struct.h>
#include <Poco/JSON/Parser.h>

#include <filesystem>
#include <exception>
#include <fstream>
#include <utility>

Q_LOGGING_CATEGORY(lcIpcClient, "gui.v4.ipc", QtInfoMsg)

namespace {
constexpr uint16_t initialConnectionRetryDelayMs = 2000;
constexpr uint8_t initialConnectionLogEveryAttempts = 10;
} // namespace

namespace KDC {

IpcClient::IpcClient(QObject *parent) :
    QObject(parent),
    _socket(new QTcpSocket(this)),
    _initialConnectionRetryTimer(this) {
    _initialConnectionRetryTimer.setSingleShot(true);
    (void) connect(&_initialConnectionRetryTimer, &QTimer::timeout, this, &IpcClient::attemptInitialConnection);
    (void) connect(_socket, &QTcpSocket::connected, this, &IpcClient::onConnected);
    (void) connect(_socket, &QTcpSocket::disconnected, this, &IpcClient::onDisconnected);
    (void) connect(_socket, &QTcpSocket::errorOccurred, this, &IpcClient::onErrorOccurred);
    (void) connect(_socket, &QTcpSocket::readyRead, this, &IpcClient::onReadyRead);
}


#ifdef QT_DEBUG
/**
 * In debug mode, the port is read from the .comm file created by the server.
 * The first connection is retried until it succeeds once.
 */
void IpcClient::connectToServer() {
    _initialConnectionAttemptCount = 0;
    attemptInitialConnection();
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
    if (!(commFile >> port)) {
        qCWarning(lcIpcClient) << "Failed to parse port from .comm file (corrupted content?)";
        return 0;
    }
    return port;
}

#else
/**
 * In release mode, the port is passed as a command-line argument by the server when launching the client.
 * The first connection is retried until it succeeds once.
 */
void IpcClient::connectToServer(quint16 port) {
    _configuredPort = port;
    _initialConnectionAttemptCount = 0;
    attemptInitialConnection();
}
#endif

void IpcClient::attemptInitialConnection() {
    if (_hasConnectedOnce) {
        return;
    }

    ++_initialConnectionAttemptCount;

#ifdef QT_DEBUG
    const quint16 port = readPortFromCommFile();
#else
    const quint16 port = _configuredPort;
#endif

    if (port == 0) {
        scheduleInitialConnectionRetry("Server port is not available yet");
        return;
    }

    if (_initialConnectionAttemptCount == 1 || _initialConnectionAttemptCount % initialConnectionLogEveryAttempts == 0) {
        qCInfo(lcIpcClient) << "Attempting initial IPC connection on port" << port << "- attempt"
                            << _initialConnectionAttemptCount;
    }

    _socket->abort();
    _socket->connectToHost(QHostAddress::LocalHost, port);
}

/**
 * @param num Request number (see RequestNum enum in src/libcommon/comm.h)
 * @param params Request parameters
 * @param callback Optional callback invoked with the server response. If null, the response is silently discarded.
 * @return the request ID, which can be used to match the response with the request.
 * @note Any communication error (socket not connected, serialization failure, partial write) is considered fatal: the client
 * calls exit(EXIT_FAILURE).
 * @note Callbacks are invoked on the Qt event loop. If the callback captures a raw `this`, the caller must ensure the object
 * outlives the response. Use `QPointer<T>` to guard against dangling pointers when lifetime is uncertain.
 */
int32_t IpcClient::sendRequest(const RequestNum num, const Poco::DynamicStruct &params, ResponseCallback callback) {
    if (_socket->state() != QAbstractSocket::ConnectedState) {
        qCCritical(lcIpcClient) << "Cannot send request, socket not in ConnectedState mode (state: " << _socket->state()
                                << ")"; // See qabstractsocket.h#SocketState
        exit(EXIT_FAILURE);
    }
    const int32_t id = _nextId++;

    Poco::DynamicStruct ipcMessage;
    ipcMessage[MSG_TYPE] = toInt(GuiJobType::Query);
    ipcMessage[MSG_REQUEST_ID] = id;
    ipcMessage[MSG_REQUEST_NUM] = toInt(num);

    if (const bool insertResult = ipcMessage.insert(MSG_REQUEST_PARAMS, params).second; !insertResult) {
        qCCritical(lcIpcClient) << "Failed to insert request parameters into message";
        exit(EXIT_FAILURE);
    }

    const std::string json = Poco::Dynamic::structToString(ipcMessage);
    const auto jsonSize = static_cast<qint64>(json.size());

    if (const qint64 writtenData = _socket->write(json.data(), jsonSize); writtenData != jsonSize) {
        if (writtenData < 0) {
            qCCritical(lcIpcClient) << "Failed to send request, error:" << _socket->errorString();
        } else {
            // Very unlikely: the kernel buffer is filled by an internal write to the fd; if the server is not reading in time,
            // another issue is probably occurring on its side.
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
    if (!_hasConnectedOnce) {
        qCInfo(lcIpcClient) << "Initial IPC connection established after" << _initialConnectionAttemptCount << "attempt(s)";
    }

    _hasConnectedOnce = true;
    _initialConnectionRetryTimer.stop();
    emit connected();
}

void IpcClient::onDisconnected() {
    if (!_hasConnectedOnce) {
        scheduleInitialConnectionRetry("Socket disconnected before the first successful connection");
        return;
    }
    qCWarning(lcIpcClient) << "Socket disconnected";
    emit disconnected();
    exit(EXIT_FAILURE);
}

void IpcClient::onErrorOccurred(const QAbstractSocket::SocketError socketError) {
    if (!_hasConnectedOnce) {
        scheduleInitialConnectionRetry(QString("Socket error %1 - %2").arg(toInt(socketError)).arg(_socket->errorString()));
        return;
    }

    qCCritical(lcIpcClient) << "Socket error:" << socketError << "-" << _socket->errorString();
    qCCritical(lcIpcClient) << "This error is considered fatal, exiting.";
    exit(EXIT_FAILURE);
}

/** Appends incoming bytes to the read buffer and triggers message extraction with IpcClient::processBuffer. */
void IpcClient::onReadyRead() {
    const QByteArray bytes = _socket->readAll();
    (void) _readBuffer.append(bytes.constData(), static_cast<size_t>(bytes.size()));
    processBuffer();
}

void IpcClient::scheduleInitialConnectionRetry(const QString &reason) {
    if (_hasConnectedOnce || _initialConnectionRetryTimer.isActive()) {
        return;
    }

    if (_initialConnectionAttemptCount == 1) {
        qCInfo(lcIpcClient) << "Initial IPC connection not ready yet:" << reason << "- retrying in"
                            << initialConnectionRetryDelayMs << "ms";
    } else if (_initialConnectionAttemptCount % initialConnectionLogEveryAttempts == 0) {
        qCWarning(lcIpcClient) << "Initial connection still unavailable after" << _initialConnectionAttemptCount
                               << "attempts:" << reason << "- retrying in" << initialConnectionRetryDelayMs << "ms";
    }

    _initialConnectionRetryTimer.start(initialConnectionRetryDelayMs);
}

/**
 * Handles a query response (`type:1`) after generic message parsing.
 *
 * Validates the response-specific fields (`code`, `cause`), rebuilds the
 * corresponding `ExitInfo`, then resolves the pending callback identified by
 * @p id. The callback is removed from `_pendingCallbacks` before invocation so
 * it is always cleaned up, even if the user code throws.
 *
 * @param ipcMessage Fully parsed IPC message envelope.
 * @param id         Request identifier used to correlate the response.
 */
void IpcClient::handleResponseMessage(const Poco::DynamicStruct &ipcMessage, const int32_t id) {
    if (!ipcMessage.contains(MSG_RESPONSE_CODE) || !ipcMessage.contains(MSG_RESPONSE_CAUSE)) {
        qCCritical(lcIpcClient) << "Response missing code/cause fields for id:" << id;
        exit(EXIT_FAILURE);
    }

    auto requestNum = RequestNum::Unknown;
    CommonUtility::readValueFromStruct(ipcMessage, MSG_REQUEST_NUM, requestNum);

    qCDebug(lcIpcClient) << "Reply received | RequestNum:" << requestNum << "/ id:" << id;
    if (const auto it = _pendingCallbacks.find(id); it != _pendingCallbacks.end()) {
        if (!ipcMessage[MSG_REQUEST_PARAMS].isStruct()) {
            qCCritical(lcIpcClient) << "params field is not a JSON object for id:" << id;
            exit(EXIT_FAILURE);
        }
        const Poco::DynamicStruct params = ipcMessage[MSG_REQUEST_PARAMS].extract<Poco::DynamicStruct>();

        auto exitCode = ExitCode::Unknown;
        CommonUtility::readValueFromStruct(ipcMessage, MSG_RESPONSE_CODE, exitCode);

        auto exitCause = ExitCause::Unknown;
        CommonUtility::readValueFromStruct(ipcMessage, MSG_RESPONSE_CAUSE, exitCause);

        const auto callback = std::move(it.value());
        _pendingCallbacks.erase(it);

        try {
            callback(ExitInfo(exitCode, exitCause), params);
        } catch (const std::exception &e) {
            qCCritical(lcIpcClient) << "Exception in response callback for request id:" << id << "(RequestNum:" << requestNum
                                    << ") -" << e.what();
        } catch (...) {
            qCCritical(lcIpcClient) << "Unknown exception in response callback for request id:" << id
                                    << "(RequestNum:" << requestNum << ")";
        }
    } else {
        qCWarning(lcIpcClient) << "Received response for unknown request id:" << id;
    }
}

/**
 * Handles a server-initiated signal (`type:2`) after generic message parsing.
 *
 * Extracts the `SignalNum` from the shared message envelope and forwards the
 * payload to upper layers through `serverSignalReceived`.
 *
 * @param ipcMessage Fully parsed IPC message envelope.
 * @param id         Signal identifier assigned by the server.
 */
void IpcClient::handleServerSignal(const Poco::DynamicStruct &ipcMessage, const int32_t id) {
    auto num = SignalNum::Unknown;
    CommonUtility::readValueFromStruct(ipcMessage, MSG_REQUEST_NUM, num);

    if (!ipcMessage[MSG_REQUEST_PARAMS].isStruct()) {
        qCCritical(lcIpcClient) << "params field is not a JSON object for signal id:" << id;
        exit(EXIT_FAILURE);
    }
    const Poco::DynamicStruct params = ipcMessage[MSG_REQUEST_PARAMS].extract<Poco::DynamicStruct>();

    qCDebug(lcIpcClient) << "Signal received | SignalNum:" << num << "/ id:" << id;
    emit serverSignalReceived(num, params);
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
        try {
            parser.reset();
            const Poco::Dynamic::Var var = parser.parse(raw);
            const auto &objPtr = var.extract<Poco::JSON::Object::Ptr>();
            if (!objPtr) {
                qCCritical(lcIpcClient) << "Failed to parse the JSON message: \n'" << raw << "'\n";
                exit(EXIT_FAILURE);
            }
            const Poco::DynamicStruct ipcMessage = *objPtr;

            if (!ipcMessage.contains(MSG_TYPE) || !ipcMessage.contains(MSG_REQUEST_ID) || !ipcMessage.contains(MSG_REQUEST_NUM) ||
                !ipcMessage.contains(MSG_REQUEST_PARAMS)) {
                qCCritical(lcIpcClient) << "Received malformed message, missing required fields";
                exit(EXIT_FAILURE);
            }

            auto type = GuiJobType::Unknown;
            CommonUtility::readValueFromStruct(ipcMessage, MSG_TYPE, type);

            int32_t id = 0;
            CommonUtility::readValueFromStruct(ipcMessage, MSG_REQUEST_ID, id);

            switch (type) {
                case GuiJobType::Query: {
                    handleResponseMessage(ipcMessage, id);
                    break;
                }
                case GuiJobType::Signal: {
                    handleServerSignal(ipcMessage, id);
                    break;
                }
                default:
                    qCCritical(lcIpcClient) << "Received message with unknown type:" << type;
                    exit(EXIT_FAILURE);
            }
        } catch (const Poco::Exception &e) {
            qCCritical(lcIpcClient) << "Poco exception while processing message:" << e.what();
            exit(EXIT_FAILURE);
        } catch (const std::exception &e) {
            qCCritical(lcIpcClient) << "Std Exception while processing message:" << e.what();
            exit(EXIT_FAILURE);
        }
    }
}

/**
 * Extract the first complete JSON message from the buffer.
 * Here, we can count the brackets because every string is base64 encoded so it cannot contain brackets, and there are no escape
 * characters in the JSON messages.
 * @param buffer The buffer containing one or more JSON messages. Each message is expected to be a JSON object starting with '{'
 * and ending with the matching '}'.
 * @param outMessage The extracted JSON message, if a complete message is found at the beginning of the buffer. The message is
 * removed from the buffer.
 * @return true if a complete message was successfully extracted and removed from the buffer, false if the buffer is empty or does
 * not yet contain a complete JSON object.
 * @note Malformed input (buffer not starting with '{', or too many nested objects) is considered fatal: the client calls
 * exit(EXIT_FAILURE).
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
            if (balance ==
                std::numeric_limits<decltype(balance)>::max()) { // Avoid overflow of balance variable, which would cause
                                                                 // incorrect parsing and potential security issues
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
