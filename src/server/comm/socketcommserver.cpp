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

SocketCommChannel::SocketCommChannel() :
    AbstractCommChannel() {}

SocketCommChannel::~SocketCommChannel() {}

uint64_t SocketCommChannel::readData(CommChar *data, uint64_t maxlen) {
    try {
        auto bytesReceived = _socket.receiveBytes(data, static_cast<int>(maxlen) * sizeof(CommChar));
        if (bytesReceived == 0) {
            LOG_DEBUG(Log::instance()->getLogger(), "Socket connection closed by peer");
            lostConnectionCbk();
            close();
            return 0;
        }
        return bytesReceived;
    } catch (Poco::IOException &ex) {
        LOG_ERROR(Log::instance()->getLogger(), "Socket receiveBytes error: " << ex.displayText());
        lostConnectionCbk();
        close();
        return 0;
    }
}

uint64_t SocketCommChannel::writeData(const CommChar *data, uint64_t len) {
    try {
        return _socket.sendBytes(data, static_cast<int>(len) * sizeof(CommChar));
    } catch (Poco::IOException &ex) {
        LOG_ERROR(Log::instance()->getLogger(), "Socket sendBytes error: " << ex.displayText());
        lostConnectionCbk();
        close();
        return 0;
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
    _socket.close();
}

SocketCommServer::SocketCommServer(const std::string &name) :
    AbstractCommServer(name) {}

SocketCommServer::~SocketCommServer() {
    close();
}

void SocketCommServer::close() {
    if (_stopAsked) {
        LOG_DEBUG(Log::instance()->getLogger(), name() << " is already stoping");
        return;
    } else if (_isListening) {
        LOG_DEBUG(Log::instance()->getLogger(), name() << " is stopping");
        _stopAsked = true;
        Poco::Net::StreamSocket socket;
        socket.connect(Poco::Net::SocketAddress("localhost", 12345)); // Connect to unblock accept
        if (_serverSocketThread && _serverSocketThread->joinable()) {
            _serverSocketThread->join();
        }
        if (_readyReadCbkThread && _readyReadCbkThread->joinable()) {
            _readyReadCbkThread->join();
        }
        LOG_DEBUG(Log::instance()->getLogger(), name() << " stopped");
        _isListening = false;
        _stopAsked = false;
    } else if (!_isListening) {
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
    _isListening = true;
    _serverSocketThread = (std::make_unique<std::thread>(executeFunc, this));
    _readyReadCbkThread = (std::make_unique<std::thread>(readyReadCbkHandlerFunc, this));
    return true;
}

std::shared_ptr<AbstractCommChannel> SocketCommServer::nextPendingConnection() {
    std::scoped_lock(_channelsMutex);
    return _channels.back();
}

std::list<std::shared_ptr<AbstractCommChannel>> SocketCommServer::connections() {
    std::scoped_lock(_channelsMutex);
    return _channels;
}

void SocketCommServer::executeFunc(SocketCommServer *server) {
    server->execute();
    log4cplus::threadCleanup();
}

void SocketCommServer::readyReadCbkHandler() {
    while (!_stopAsked) {
        std::scoped_lock(_channelsMutex);
        for (auto channel: _channels) {
            if (channel->bytesAvailable() > 0) {
                channel->readyReadCbk();
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void SocketCommServer::readyReadCbkHandlerFunc(SocketCommServer *server) {
    server->readyReadCbkHandler();
    log4cplus::threadCleanup();
}

void SocketCommServer::execute() {
    _serverSocket.bind(Poco::Net::SocketAddress("localhost", 12345), true);
    while (!_stopAsked) {
        _serverSocket.listen(10);
        Poco::Net::StreamSocket socket = _serverSocket.acceptConnection();
        if (_stopAsked) break;
        auto channel = makeCommChannel();
        auto socketChannel = std::dynamic_pointer_cast<SocketCommChannel>(channel);
        std::scoped_lock(_channelsMutex);
        socketChannel->setSocket(std::move(socket));
        socketChannel->setLostConnectionCbk([this](std::shared_ptr<AbstractCommChannel> ch) {
            lostConnectionCbk(ch);
            std::scoped_lock(_channelsMutex);
            _channels.remove(ch);
        });
        _channels.push_back(socketChannel);
        newConnectionCbk();
    }
}

} // namespace KDC
