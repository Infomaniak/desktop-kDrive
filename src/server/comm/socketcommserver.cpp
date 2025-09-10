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
        auto lenReceived = _socket.receiveBytes(data, static_cast<int>(maxlen) * sizeof(CommChar)) / sizeof(CommChar);
        if (lenReceived == 0) {
            LOG_DEBUG(Log::instance()->getLogger(), "Socket connection closed by peer");
            lostConnectionCbk();
            close();
            return 0;
        }
        return lenReceived;
    } catch (Poco::IOException &ex) {
        LOG_ERROR(Log::instance()->getLogger(), "Socket receiveBytes error: " << ex.displayText());
        lostConnectionCbk();
        close();
        return 0;
    }
}

uint64_t SocketCommChannel::writeData(const CommChar *data, uint64_t len) {
    try {
        return _socket.sendBytes(data, static_cast<int>(len) * sizeof(CommChar)) / sizeof(CommChar);
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

            readyReadCbk();
            while (_socket.available() > 0 && !_isClosing) {
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
        return _socket.available();
    } catch (Poco::IOException &ex) {
        LOG_ERROR(Log::instance()->getLogger(), "Socket available error: " << ex.displayText());
        return 0;
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

bool SocketCommServer::listen(const KDC::SyncPath &) {
    if (_isListening) {
        LOG_DEBUG(Log::instance()->getLogger(), name() << " is already listening");
        return false;
    }

    LOG_DEBUG(Log::instance()->getLogger(), name() << " start");
    _serverSocketThread = std::make_unique<std::thread>(&SocketCommServer::execute, this);
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

void SocketCommServer::execute() {
    _serverSocket.bind(Poco::Net::SocketAddress("localhost", "0"), true, true);
    while (!_stopAsked) {
        _serverSocket.listen(1);
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
