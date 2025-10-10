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

#include "socketcommserver.h"

namespace KDC {

SocketCommChannel::SocketCommChannel(const Poco::Net::StreamSocket &socket) :
    AbstractCommChannel(),
    _socket(socket) {
    _callbackThread = std::thread(&SocketCommChannel::callbackHandler, this);
}

SocketCommChannel::~SocketCommChannel() {
    _isClosing = true;
    try {
        _socket.shutdown();
    } catch (Poco::IOException &ex) {
        LOG_DEBUG(Log::instance()->getLogger(), "Socket shutdown error: " << ex.displayText());
    }
    if (_callbackThread.joinable()) {
        _callbackThread.join();
    }
}

uint64_t SocketCommChannel::readData(CommChar *data, uint64_t maxlen) {
    try {
        /* This file is included by some files that also include "windef.h", which defines a macro "max()".
         * This causes std::numeric_limits<int>::max() to be expanded as
         * std::numeric_limits<int>::(((a) > (b)) ? (a) : (b)), which obviously doesn't compile.
         * The following pragma allows temporarily disabling this macro.
         */
#pragma push_macro("max")
#undef max
        maxlen = (std::min<uint64_t>) (maxlen * sizeof(CommChar), static_cast<uint64_t>(std::numeric_limits<int>::max()));
        const int lenReceived = _socket.receiveBytes(data, static_cast<int>(maxlen));
#pragma pop_macro("max")

        if (lenReceived <= 0) {
            LOG_DEBUG(Log::instance()->getLogger(),
                      (lenReceived == 0 ? "Socket connection closed by peer" : "Socket connection error"));
            lostConnectionCbk();
            close();
            _pendingRead = false;
            return 0;
        }
        _pendingRead = false;
        return static_cast<unsigned int>(lenReceived) / sizeof(CommChar);
    } catch (Poco::IOException &ex) {
        LOG_ERROR(Log::instance()->getLogger(), "Socket receiveBytes error: " << ex.displayText());
        lostConnectionCbk();
        close();
        _pendingRead = false;
        return 0;
    }
}

uint64_t SocketCommChannel::writeData(const CommChar *data, uint64_t len) {
    try {
#pragma push_macro("max")
#undef max
        if (len > std::numeric_limits<int>::max() / sizeof(CommChar)) {
#pragma pop_macro("max")
            LOG_ERROR(Log::instance()->getLogger(), "Socket writeData error: data length too large");
            return 0;
        }
        const int commCharSize = sizeof(CommChar);
        const auto written = _socket.sendBytes(data, static_cast<int>(len) * commCharSize);
        if (written < 0) {
            LOG_ERROR(Log::instance()->getLogger(), "Socket connection error on sendBytes");
            lostConnectionCbk();
            close();
            return 0;
        }
        return static_cast<uint64_t>(written / commCharSize);
    } catch (Poco::IOException &ex) {
        LOG_ERROR(Log::instance()->getLogger(), "Socket sendBytes error: " << ex.displayText());
        return 0;
    }
}

void SocketCommChannel::callbackHandler() {
    while (!_isClosing) {
        try {
            if (!_socket.poll(Poco::Timespan(1, 0), Poco::Net::Socket::SELECT_READ | Poco::Net::Socket::SELECT_ERROR)) {
                continue;
            }

            if (_socket.available() == 0) {
                LOG_DEBUG(Log::instance()->getLogger(), "Socket connection closed by peer");
                lostConnectionCbk();
                break;
            }
            _pendingRead = true;
            readyReadCbk();
            while (!_isClosing && _pendingRead) { // Wait until readData is called before polling again
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        } catch (Poco::IOException &ex) {
            LOG_ERROR(Log::instance()->getLogger(), "Socket poll error: " << ex.displayText());
            lostConnectionCbk();
            break;
        }
    }
}

uint64_t SocketCommChannel::bytesAvailable() const {
    try {
        return static_cast<uint64_t>((std::max)(0, _socket.available()));
    } catch (Poco::IOException &ex) {
        LOG_ERROR(Log::instance()->getLogger(), "Socket available error: " << ex.displayText());
        return static_cast<uint64_t>(0);
    }
}

void SocketCommChannel::close() {
    _socket.shutdown();
}

SocketCommServer::SocketCommServer(const std::string &name) :
    AbstractCommServer(name) {}

SocketCommServer::~SocketCommServer() {
    try {
        close();
    } catch (std::exception &ex) {
        // Avoid exceptions in destructor
        LOG_ERROR(Log::instance()->getLogger(), "Exception in SocketCommServer destructor: " << ex.what());
    }
}

void SocketCommServer::close() {
    if (_stopAsked) {
        LOG_DEBUG(Log::instance()->getLogger(), name() << " is already stoping");
        return;
    } else if (_isListening) {
        LOG_DEBUG(Log::instance()->getLogger(), name() << " is stopping");
        _stopAsked = true;
        if (!_serverSocket.isNull()) {
            Poco::Net::StreamSocket socket;
            socket.connect(Poco::Net::SocketAddress("localhost", getPort())); // Connect to unblock accept
        }

        if (_serverSocketThread && _serverSocketThread->joinable()) {
            _serverSocketThread->join();
        }
        LOG_DEBUG(Log::instance()->getLogger(), name() << " stopped");
        _isListening = false;
        _stopAsked = false;
    } else {
        LOG_DEBUG(Log::instance()->getLogger(), name() << " is not listening");
        return;
    }
}

bool SocketCommServer::listen() {
    if (_isListening) {
        LOG_DEBUG(Log::instance()->getLogger(), name() << " is already listening");
        return false;
    }

    LOG_DEBUG(Log::instance()->getLogger(), name() << " start");
    _serverSocketThread = std::make_unique<std::thread>(&SocketCommServer::execute, this);

    // Wait until the server socket is listening (or timeout after 20s)
    int remainTries = 500;
    while (!_isListening && remainTries > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        --remainTries;
    }
    return remainTries > 0;
}

std::shared_ptr<AbstractCommChannel> SocketCommServer::nextPendingConnection() {
    const std::scoped_lock lock(_channelsMutex);
    return _channels.back();
}

std::list<std::shared_ptr<AbstractCommChannel>> SocketCommServer::connections() {
    const std::scoped_lock lock(_channelsMutex);
    return _channels;
}

void saveCommPort(unsigned short port) {
    // Build the path: $HOME/.comm
    std::filesystem::path commPath = std::filesystem::path(std::getenv("USERPROFILE")) / ".comm4";

    // Open the file and write the port
    std::ofstream commFile(commPath, std::ios_base::trunc | std::ios_base::out);
    if (commFile.is_open()) {
        commFile << port;
        commFile.close();
    }
}
void SocketCommServer::execute() {
    _serverSocket.bind(Poco::Net::SocketAddress("localhost", "0"), true, true);
    LOG_DEBUG(Log::instance()->getLogger(), name() << " listening on port " << getPort());
    saveCommPort(getPort());
    while (!_stopAsked) {
        _serverSocket.listen();
        _isListening = true;
        Poco::Net::StreamSocket socket = _serverSocket.acceptConnection();
        if (_stopAsked) break;
        auto channel = makeCommChannel(socket);
        const std::scoped_lock lock(_channelsMutex);
        channel->setLostConnectionCbk([this](std::shared_ptr<AbstractCommChannel> ch) { lostConnectionCbk(ch); });
        _channels.push_back(channel);
        newConnectionCbk();
    }
    _isListening = false;
    log4cplus::threadCleanup();
}
} // namespace KDC
