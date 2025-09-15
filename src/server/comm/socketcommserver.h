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

#include <Poco/Net/ServerSocket.h>

namespace KDC {

class SocketCommChannel : public AbstractCommChannel {
    public:
        SocketCommChannel();
        ~SocketCommChannel();

        uint64_t bytesAvailable() const override;

    private:
        Poco::Net::ServerSocket _socket;

        uint64_t readData(CommChar *data, uint64_t maxlen) override;
        virtual uint64_t writeData(const CommChar *data, uint64_t len) override;
};

class SocketCommServer : public AbstractCommServer {
    public:
        SocketCommServer(const std::string &name);
        ~SocketCommServer();

        void close() override;
        bool listen(const KDC::SyncPath &) override;
        std::shared_ptr<KDC::AbstractCommChannel> nextPendingConnection() override;
        std::list<std::shared_ptr<KDC::AbstractCommChannel>> connections() override;

        static bool removeServer(const KDC::SyncPath &path) {
#if defined(KD_LINUX)
            std::error_code ec;
            std::filesystem::remove(path, ec);
#endif
            return true;
        }
};

} // namespace KDC
