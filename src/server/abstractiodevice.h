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

#include <cstdint>
#include <memory>
#include <functional>

namespace KDC {

class AbstractIODevice {
    public:
        AbstractIODevice();
        virtual ~AbstractIODevice() { destroyedCbk(); }

        virtual uint64_t readData(char *data, uint64_t maxlen) = 0;
        virtual uint64_t writeData(const char *data, uint64_t len) = 0;

        virtual bool isSequential() const { return true; }
        virtual uint64_t bytesAvailable() const = 0;
        virtual bool canReadLine() const = 0;

        void setLostConnectionCbk(const std::function<void(AbstractIODevice *)> &cbk) { _onLostConnectionCbk = cbk; }
        void lostConnectionCbk() {
            if (_onLostConnectionCbk) _onLostConnectionCbk(this);
        }
        void setReadyReadCbk(const std::function<void(AbstractIODevice *)> &cbk) { _onReadyReadCbk = cbk; }
        void readyReadCbk() {
            if (_onReadyReadCbk) _onReadyReadCbk(this);
        }
        void setDestroyedCbk(const std::function<void(AbstractIODevice *)> &cbk) { _onDestroyedCbk = cbk; }
        void destroyedCbk() {
            if (_onDestroyedCbk) _onDestroyedCbk(this);
        }

    private:
        std::function<void(AbstractIODevice *)> _onLostConnectionCbk;
        std::function<void(AbstractIODevice *)> _onReadyReadCbk;
        std::function<void(AbstractIODevice *)> _onDestroyedCbk;
};

} // namespace KDC
