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

#include "abstractiodevice.h"

#include <string>
#include <functional>

namespace KDC {
class CommApi;
} // namespace KDC

class CommServerPrivate;
class CommChannelPrivate;

class CommChannel : public KDC::AbstractIODevice {
    public:
        CommChannel(CommChannelPrivate *p);
        ~CommChannel();

        uint64_t readData(char *data, uint64_t maxlen) override;
        uint64_t writeData(const char *data, uint64_t len) override;

        bool isSequential() const override { return true; }
        uint64_t bytesAvailable() const override;
        bool canReadLine() const override;

        void setLostConnectionCbk(const std::function<void()> &cbk) { _onLostConnectionFct = cbk; }
        void lostConnectionCbk() {
            if (_onLostConnectionFct) _onLostConnectionFct();
        }

    private:
        std::unique_ptr<CommChannelPrivate> d_ptr;
        std::function<void()> _onLostConnectionFct;

        friend class CommServerPrivate;
};

class CommServer {
    public:
        CommServer();
        ~CommServer();

        void close();
        bool listen(const std::string &name);
        CommChannel *nextPendingConnection();
        CommChannel *guiConnection();

        static bool removeServer(const std::string &) { return false; }

        void setNewExtConnectionCbk(const std::function<void()> &cbk) { _onNewExtConnectionFct = cbk; }
        void newExtConnectionCbk() {
            if (_onNewExtConnectionFct) _onNewExtConnectionFct();
        }
        void setNewGuiConnectionCbk(const std::function<void()> &cbk) { _onNewGuiConnectionFct = cbk; }
        void newGuiConnectionCbk() {
            if (_onNewGuiConnectionFct) _onNewGuiConnectionFct();
        }
        void setLostExtConnectionCbk(const std::function<void()> &cbk) { _onLostExtConnectionFct = cbk; }
        void lostExtConnectionCbk() {
            if (_onLostExtConnectionFct) _onLostExtConnectionFct();
        }
        void setLostGuiConnectionCbk(const std::function<void()> &cbk) { _onLostGuiConnectionFct = cbk; }
        void lostGuiConnectionCbk() {
            if (_onLostGuiConnectionFct) _onLostGuiConnectionFct();
        }

    private:
        std::unique_ptr<CommServerPrivate> d_ptr;
        std::function<void()> _onNewExtConnectionFct;
        std::function<void()> _onNewGuiConnectionFct;
        std::function<void()> _onLostExtConnectionFct;
        std::function<void()> _onLostGuiConnectionFct;
};
