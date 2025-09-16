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

#include "abstractcommchannel.h"
#include "log/log.h"

namespace KDC {

class AbstractCommServer {
    public:
        AbstractCommServer(const std::string &name) :
            _name(name) {
            LOG_DEBUG(Log::instance()->getLogger(), _name << " created");
        }

        virtual ~AbstractCommServer() {
            LOG_DEBUG(Log::instance()->getLogger(), _name << " destroyed");
            log4cplus::threadCleanup();
        }

        std::string name() { return _name; }

        virtual void close() = 0;
        /**
         * @brief start server
         * @param pipePath is the path of the pipe for a server accepting pipe connections
         */
        virtual bool listen(const SyncPath &pipePath) = 0;
        virtual std::shared_ptr<AbstractCommChannel> nextPendingConnection() = 0;
        virtual std::list<std::shared_ptr<AbstractCommChannel>> connections() = 0;

        static bool removeServer(const SyncPath &) { return true; }

        void setNewConnectionCbk(const std::function<void()> &cbk) { _onNewConnectionCbk = cbk; }
        void newConnectionCbk() {
            if (_onNewConnectionCbk) _onNewConnectionCbk();
        }

        void setLostConnectionCbk(const CommChannelCallback &cbk) { _onLostConnectionCbk = cbk; }
        void lostConnectionCbk(std::shared_ptr<AbstractCommChannel> channel) {
            if (_onLostConnectionCbk) _onLostConnectionCbk(channel);
        }

    private:
        std::string _name;
        std::function<void()> _onNewConnectionCbk;
        CommChannelCallback _onLostConnectionCbk;
};

} // namespace KDC
