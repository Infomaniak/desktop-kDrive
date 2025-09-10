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

#pragma once

#include "abstractcommserver.h"
#include "libcommon/utility/types.h"

#include <Poco/Net/Socket.h>
#include <Poco/Net/ServerSocket.h>

namespace KDC {

class SocketCommChannel : public AbstractCommChannel {
    public:
        explicit SocketCommChannel(const Poco::Net::StreamSocket &socket);
        ~SocketCommChannel();

        uint64_t bytesAvailable() const override;
        void close() override;

    protected:
        // Return number of CommChar (/!\ not always equal the number of bytes) read or 0 on error or closed connection
        uint64_t readData(CommChar *data, uint64_t maxlen) override;

        // Return number of CommChar (/!\ not always equal the number of bytes) written
        uint64_t writeData(const CommChar *data, uint64_t len) override;

    private:
        bool _isClosing = false;
        std::thread _callbackThread;
        Poco::Net::StreamSocket _socket;

        void callbackHandler();
};

class SocketCommServer : public AbstractCommServer {
    public:
        SocketCommServer(const std::string &name);
        ~SocketCommServer();
        int getPort() const { return static_cast<int>(_serverSocket.address().port()); }
        void close() final;
        bool listen(const SyncPath &) override;
        std::shared_ptr<AbstractCommChannel> nextPendingConnection() override;
        std::list<std::shared_ptr<AbstractCommChannel>> connections() override;

    protected:
        virtual std::shared_ptr<SocketCommChannel> makeCommChannel(Poco::Net::StreamSocket &socket) const = 0;

    private:
        Poco::Net::ServerSocket _serverSocket;
        std::mutex _channelsMutex;
        std::list<std::shared_ptr<AbstractCommChannel>> _channels;
        bool _isListening = false;
        bool _stopAsked = false;
        std::unique_ptr<std::thread> _serverSocketThread;
        void execute();
};

} // namespace KDC
