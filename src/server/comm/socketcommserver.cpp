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

SocketCommChannel::SocketCommChannel(Poco::Net::StreamSocket socket) :
    AbstractCommChannel() {}

SocketCommChannel::~SocketCommChannel() {
    void destroyedCbk();
}

uint64_t SocketCommChannel::readData(CommChar *data, uint64_t maxlen) {
    try {
        return _socket.receiveBytes(data, static_cast<int>(maxlen));
    } catch (Poco::IOException &ex) {
        LOG_ERROR(Log::instance()->getLogger(), "Socket receiveBytes error: " << ex.displayText());
        return 0;
    }
}

uint64_t SocketCommChannel::writeData(const CommChar *data, uint64_t len) {
    try {
        return _socket.sendBytes(data, static_cast<int>(len));
    } catch (Poco::IOException &ex) {
        LOG_ERROR(Log::instance()->getLogger(), "Socket sendBytes error: " << ex.displayText());
        return 0;
    }
}

uint64_t SocketCommChannel::bytesAvailable() const {
    return _socket.available();
}

SocketCommServer::SocketCommServer(const std::string &name) :
    AbstractCommServer(name) {}

SocketCommServer::~SocketCommServer() {}

void SocketCommServer::close() {
    if (_stopAsked) {
        LOG_DEBUG(Log::instance()->getLogger(), name() << " is already stoping");
        return;
    } else if (_isListening) {
        LOG_DEBUG(Log::instance()->getLogger(), name() << " is stopping");
        _stopAsked = true;
        if (_thread && _thread->joinable()) {
            _thread->join();
            LOG_DEBUG(Log::instance()->getLogger(), name() << " stopped");
        }
        _isListening = false;
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
    _thread = (std::make_unique<std::thread>(executeFunc, this));
}

std::shared_ptr<AbstractCommChannel> SocketCommServer::nextPendingConnection() {
    if (_channels.empty()) return nullptr;
    auto channel = _channels.back();
    _channels.pop_back();
    return channel;
}

std::list<std::shared_ptr<AbstractCommChannel>> SocketCommServer::connections() {
    return _channels;
}

void SocketCommServer::executeFunc(SocketCommServer *server) {
    server->execute();
    log4cplus::threadCleanup();
}

void SocketCommServer::execute() {
    while (!_stopAsked) {
        _serverSocket.listen(1);
        Poco::Net::StreamSocket socket = _serverSocket.acceptConnection();
        auto channel = std::make_shared<SocketCommChannel>(std::move(socket));
        _channels.push_back(channel);
        newConnectionCbk();
    }
    _stopAsked = false;
}

} // namespace KDC
