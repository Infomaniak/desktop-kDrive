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

namespace KDC {

class AbstractCommServer {
    public:
        AbstractCommServer() = default;
        virtual ~AbstractCommServer() {}

        virtual void close() = 0;
        virtual bool listen(const std::string &name) = 0;
        virtual std::shared_ptr<AbstractCommChannel> nextPendingConnection() = 0;
        virtual std::list<std::shared_ptr<AbstractCommChannel>> extConnections() = 0;
        virtual std::shared_ptr<AbstractCommChannel> guiConnection() = 0;

        static bool removeServer(const std::string &) { return true; }

        void setNewExtConnectionCbk(const std::function<void()> &cbk) { _onNewExtConnectionCbk = cbk; }
        void newExtConnectionCbk() {
            if (_onNewExtConnectionCbk) _onNewExtConnectionCbk();
        }
        void setNewGuiConnectionCbk(const std::function<void()> &cbk) { _onNewGuiConnectionCbk = cbk; }
        void newGuiConnectionCbk() {
            if (_onNewGuiConnectionCbk) _onNewGuiConnectionCbk();
        }

        void setLostExtConnectionCbk(const std::function<void(std::shared_ptr<AbstractCommChannel>)> &cbk) {
            _onLostExtConnectionCbk = cbk;
        }
        void lostExtConnectionCbk(std::shared_ptr<AbstractCommChannel> channel) {
            if (_onLostExtConnectionCbk) _onLostExtConnectionCbk(channel);
        }
        void setLostGuiConnectionCbk(const std::function<void(std::shared_ptr<AbstractCommChannel>)> &cbk) {
            _onLostGuiConnectionCbk = cbk;
        }
        void lostGuiConnectionCbk(std::shared_ptr<AbstractCommChannel> channel) {
            if (_onLostGuiConnectionCbk) _onLostGuiConnectionCbk(channel);
        }

    private:
        std::function<void()> _onNewExtConnectionCbk;
        std::function<void()> _onNewGuiConnectionCbk;
        std::function<void(std::shared_ptr<AbstractCommChannel>)> _onLostExtConnectionCbk;
        std::function<void(std::shared_ptr<AbstractCommChannel>)> _onLostGuiConnectionCbk;
};

} // namespace KDC
