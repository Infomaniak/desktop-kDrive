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

#include "libcommon/utility/types.h"

#include <cstdint>
#include <memory>
#include <functional>
#include <filesystem>

namespace KDC {

class AbstractCommChannel : public std::enable_shared_from_this<AbstractCommChannel> {
    public:
        AbstractCommChannel() = default;
        virtual ~AbstractCommChannel();

        bool open();
        void close();
        void sendMessage(const CommString &message);
        CommString readLine();

        //! Reads from the device.
        /*!
          \param data is a char array pointer.
          \param maxSize is is the maximum number of bytes to read.
          \return the number of bytes read or -1 if an error occurred.
        */
        virtual uint64_t readData(char *data, uint64_t maxSize) = 0;

        //! Writes to the device.
        /*!
          \param data is a char array pointer.
          \param maxSize is is the maximum number of bytes to write.
          \return the number of bytes written or -1 if an error occurred.
        */
        virtual uint64_t writeData(const char *data, uint64_t maxSize) = 0;

        //! Gets from the device if anything is available for reading.
        /*!
          \return the number of bytes that are available for reading.
        */
        virtual uint64_t bytesAvailable() const = 0;

        //! Gets from the device if a line is available for reading.
        /*!
          \return true if a complete line of data can be read from the device; otherwise returns false.
        */
        virtual bool canReadLine() const = 0;

        //! Gets a device ID.
        /*!
          \return the device ID.
        */
        virtual std::string id() const = 0;

        // Callbacks
        void setLostConnectionCbk(const std::function<void(std::shared_ptr<AbstractCommChannel>)> &cbk) {
            _onLostConnectionCbk = cbk;
        }
        void lostConnectionCbk() {
            if (_onLostConnectionCbk) _onLostConnectionCbk(shared_from_this());
        }
        void setReadyReadCbk(const std::function<void(std::shared_ptr<AbstractCommChannel>)> &cbk) { _onReadyReadCbk = cbk; }
        void readyReadCbk() {
            if (_onReadyReadCbk) _onReadyReadCbk(shared_from_this());
        }
        void setDestroyedCbk(const std::function<void(std::shared_ptr<AbstractCommChannel>)> &cbk) { _onDestroyedCbk = cbk; }
        void destroyedCbk() {
            if (_onDestroyedCbk) _onDestroyedCbk(shared_from_this());
        }

    private:
        CommString _readBuffer;
        std::function<void(std::shared_ptr<AbstractCommChannel>)> _onLostConnectionCbk;
        std::function<void(std::shared_ptr<AbstractCommChannel>)> _onReadyReadCbk;
        std::function<void(std::shared_ptr<AbstractCommChannel>)> _onDestroyedCbk;

        uint64_t write(const CommString &data);

        CommString truncateLongLogMessage(const CommString &message);
};

} // namespace KDC
