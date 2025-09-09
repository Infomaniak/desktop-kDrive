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
        SocketCommChannel();
        ~SocketCommChannel();

        uint64_t bytesAvailable() const override;
        void setSocket(Poco::Net::StreamSocket socket) { _socket = std::move(socket); };
        void close() override;
    protected:
        uint64_t readData(CommChar *data, uint64_t maxlen) final;
        virtual uint64_t writeData(const CommChar *data, uint64_t len) final;

    private:
        Poco::Net::StreamSocket _socket;
};

class SocketCommServer : public AbstractCommServer {
    public:
        SocketCommServer(const std::string &name);
        ~SocketCommServer();
        int getPort() const { return static_cast<int>(_serverSocket.address().port()); }
        void close() override;
        bool listen(const KDC::SyncPath &) override;
        std::shared_ptr<KDC::AbstractCommChannel> nextPendingConnection() override;
        std::list<std::shared_ptr<KDC::AbstractCommChannel>> connections() override;

    protected:
        virtual std::shared_ptr<SocketCommChannel> makeCommChannel() const = 0;

    private:
        Poco::Net::ServerSocket _serverSocket;
        std::mutex _channelsMutex;
        std::list<std::shared_ptr<AbstractCommChannel>> _channels;
        bool _isListening = false;
        bool _stopAsked = false;
        std::unique_ptr<std::thread> _serverSocketThread;
        std::unique_ptr<std::thread> _readyReadCbkThread;
        void execute();
        static void executeFunc(SocketCommServer *server);
        void readyReadCbkHandler();
        static void readyReadCbkHandlerFunc(SocketCommServer *server);
};

} // namespace KDC
