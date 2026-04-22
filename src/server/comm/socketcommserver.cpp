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

#include "socketcommserver.h"

#include "libcommon/utility/utility.h"
#include "libcommonserver/utility/utility.h"

namespace KDC {

constexpr char host[] = "127.0.0.1";

SocketCommChannel::SocketCommChannel(const Poco::Net::StreamSocket &socket) :
    AbstractCommChannel(),
    _socket(socket) {}

void SocketCommChannel::startCallbackThread() {
    std::weak_ptr<AbstractCommChannel> weakChannel = weak_from_this();
    auto callbackHandlerFunc = std::function<void()>([weakChannel]() {
        const auto channel = std::dynamic_pointer_cast<SocketCommChannel>(weakChannel.lock());
        if (!channel) {
            return;
        }
        channel->callbackHandler();
    });
    _callbackThread = std::make_unique<StdLoggingThread>(callbackHandlerFunc);
}

SocketCommChannel::~SocketCommChannel() {
    _isClosing = true;
    try {
        close();
    } catch (std::exception &ex) {
        LOG_ERROR(Log::instance()->getLogger(), "Exception in SocketCommChannel::close: " << ex.what());
    }

    if (_callbackThread && _callbackThread->joinable()) {
        if (_callbackThread->get_id() == std::this_thread::get_id()) {
            _callbackThread->detach();
        } else {
            _callbackThread->join();
        }
    }
}

uint64_t SocketCommChannel::readData(CommChar *data, uint64_t maxlen) {
    const auto maxSize = static_cast<int>(
            (std::min<uint64_t>) (maxlen * sizeof(CommChar), static_cast<uint64_t>(std::numeric_limits<int>::max())));
    int lenReceived = 0;
    try {
        /* This file is included by some files that also include "windef.h", which defines a macro "max()".
         * This causes std::numeric_limits<int>::max() to be expanded as
         * std::numeric_limits<int>::(((a) > (b)) ? (a) : (b)), which obviously doesn't compile.
         * The following pragma allows temporarily disabling this macro.
         */
#pragma push_macro("max")
#undef max
        lenReceived = _socket.receiveBytes(data, maxSize);
#pragma pop_macro("max")
    } catch (Poco::Exception &ex) {
        LOG_ERROR(Log::instance()->getLogger(), "Exception in StreamSocket::receiveBytes: " << ex.displayText());
        lostConnectionCbk();
        close();
        _pendingRead = false;
        return 0;
    }

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
}

uint64_t SocketCommChannel::writeData(const CommChar *data, uint64_t len) {
#pragma push_macro("max")
#undef max
    if (len > std::numeric_limits<int>::max() / sizeof(CommChar)) {
#pragma pop_macro("max")
        LOG_ERROR(Log::instance()->getLogger(), "Socket writeData error: data length too large");
        return 0;
    }

    const int commCharSize = sizeof(CommChar);
    int written = 0;
    try {
        written = _socket.sendBytes(data, static_cast<int>(len) * commCharSize);
    } catch (Poco::Exception &ex) {
        LOG_ERROR(Log::instance()->getLogger(), "Exception in StreamSocket::sendBytes: " << ex.displayText());
        return 0;
    }

    if (written < 0) {
        LOG_ERROR(Log::instance()->getLogger(), "Socket connection error on sendBytes");
        lostConnectionCbk();
        close();
        return 0;
    }

    return static_cast<uint64_t>(written / commCharSize);
}

void SocketCommChannel::callbackHandler() {
    while (!_isClosing) {
        try {
            if (!_socket.poll(Poco::Timespan(1, 0), Poco::Net::Socket::SELECT_READ | Poco::Net::Socket::SELECT_ERROR)) {
                continue;
            }
        } catch (Poco::Exception &ex) {
            LOG_ERROR(Log::instance()->getLogger(), "Exception in StreamSocket::poll: " << ex.displayText());
            lostConnectionCbk();
            break;
        }

        try {
            if (_socket.available() == 0) {
                LOG_DEBUG(Log::instance()->getLogger(), "Socket connection closed by peer");
                lostConnectionCbk();
                break;
            }
        } catch (Poco::Exception &ex) {
            LOG_ERROR(Log::instance()->getLogger(), "Exception in StreamSocket::available: " << ex.displayText());
            lostConnectionCbk();
            break;
        }

        _pendingRead = true;
        readyReadCbk();
        while (!_isClosing && _pendingRead) { // Wait until readData is called before polling again
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

uint64_t SocketCommChannel::bytesAvailable() const {
    try {
        return static_cast<uint64_t>((std::max)(0, _socket.available()));
    } catch (Poco::Exception &ex) {
        LOG_ERROR(Log::instance()->getLogger(), "Exception in StreamSocket::available: " << ex.displayText());
        return static_cast<uint64_t>(0);
    }
}

void SocketCommChannel::close() {
    try {
        _socket.shutdown();
    } catch (Poco::Exception &ex) {
        LOG_ERROR(Log::instance()->getLogger(), "Exception in StreamSocket::shutdown: " << ex.displayText());
    }
}

SocketCommServer::SocketCommServer(const std::string &name) :
    AbstractCommServer(name) {}

SocketCommServer::~SocketCommServer() {
    try {
        close();
    } catch (std::exception &ex) {
        LOG_ERROR(Log::instance()->getLogger(), "Exception in SocketCommChannel::close: " << ex.what());
    }
}

std::string SocketCommServer::getHost() {
    return host;
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
            try {
                socket.connect(Poco::Net::SocketAddress(host, getPort())); // Connect to unblock accept
            } catch (Poco::Exception &ex) {
                LOG_ERROR(Log::instance()->getLogger(), "Exception in StreamSocket::connect: " << ex.displayText());
            }
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
    auto executeFunc = std::function<void()>([this]() { execute(); });
    _serverSocketThread = std::make_unique<StdLoggingThread>(executeFunc);

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
    std::filesystem::path commPath = CommonUtility::getAppSupportDir() / ".comm";
    // Open the file and write the port
    std::ofstream commFile(commPath, std::ios_base::trunc | std::ios_base::out);
    if (commFile.is_open()) {
        commFile << port;
        commFile.close();
    }
}

void SocketCommServer::execute() {
    int retries = 0;
    bool bindSuccess = false;
    do {
        try {
            _serverSocket.bind(Poco::Net::SocketAddress(host, "0"), true, true);
            bindSuccess = true;
        } catch (Poco::Exception &ex) {
            LOG_ERROR(Log::instance()->getLogger(), "Exception in ServerSocket::bind: " << ex.displayText());
            Utility::msleep(500);
            ++retries;
            if (retries == 10) {
                sentry::Handler::captureMessage(
                        sentry::Level::Error, "SocketCommServer bind error",
                        "Failed to bind server socket after 10 retries: " + std::string(ex.displayText()));
            }
        }
    } while (!bindSuccess); // Loop until bind is successful

    LOG_DEBUG(Log::instance()->getLogger(), name() << " listening on port " << getPort());
    saveCommPort(getPort());
    while (!_stopAsked) {
        try {
            _serverSocket.listen();
        } catch (Poco::Exception &ex) {
            LOG_ERROR(Log::instance()->getLogger(), "Exception in ServerSocket::listen: " << ex.displayText());
            return;
        }

        _isListening = true;
        Poco::Net::StreamSocket socket;
        try {
            socket = _serverSocket.acceptConnection();
        } catch (Poco::Exception &ex) {
            LOG_ERROR(Log::instance()->getLogger(), "Exception in ServerSocket::acceptConnection: " << ex.displayText());
            return;
        }

        if (_stopAsked) break;

        auto channel = makeCommChannel(socket);
        channel->setLostConnectionCbk([this](std::shared_ptr<AbstractCommChannel> ch) {
            const std::scoped_lock lock(_channelsMutex);
            _channels.remove(ch);
            lostConnectionCbk(ch);
        });
        _channels.push_back(channel);
        channel->startCallbackThread();
        newConnectionCbk();
    }
    _isListening = false;
}
} // namespace KDC
