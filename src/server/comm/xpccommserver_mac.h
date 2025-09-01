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

class AbstractCommChannelPrivate;
class AbstractCommServerPrivate;

class XPCCommChannel : public KDC::AbstractCommChannel {
    public:
        XPCCommChannel(AbstractCommChannelPrivate *commChannelPrivate);
        ~XPCCommChannel();

        uint64_t readData(char *data, uint64_t maxlen) override;
        uint64_t bytesAvailable() const override;

    protected:
        std::unique_ptr<AbstractCommChannelPrivate> _privatePtr;
};

class XPCCommServer : public KDC::AbstractCommServer {
    public:
        XPCCommServer(const std::string &name, AbstractCommServerPrivate *commServerPrivate);
        ~XPCCommServer();

        void close() override;
        bool listen(const KDC::SyncPath &) override;
        std::shared_ptr<KDC::AbstractCommChannel> nextPendingConnection() override;
        std::list<std::shared_ptr<KDC::AbstractCommChannel>> connections() override;

        static bool removeServer(const KDC::SyncPath &) { return true; }

    protected:
        std::unique_ptr<AbstractCommServerPrivate> _privatePtr;
};
