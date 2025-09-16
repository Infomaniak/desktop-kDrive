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

static const auto finderExtLineSeparator = Str("\n");
static const auto finderExtQuerySeparator = Str("\\/");
static const auto guiArgSeparator = Str(";");

namespace KDC {

class AbstractCommChannel;

using CommChannelCallback = std::function<void(std::shared_ptr<AbstractCommChannel>)>;

class AbstractCommChannel : public std::enable_shared_from_this<AbstractCommChannel> {
    public:
        AbstractCommChannel() = default;
        virtual ~AbstractCommChannel();

        bool open();
        void close();
        virtual void sendMessage(const CommString &message) final;
        virtual bool canReadLine() const;
        virtual CommString readLine() final;

        //! Gets a device ID.
        /*!
          \return the device ID.
        */
        std::string id();

        //! Gets from the device if anything is available for reading.
        /*!
          \return the number of bytes that are available for reading.
        */
        virtual uint64_t bytesAvailable() const = 0;

        // Callbacks
        void setLostConnectionCbk(const CommChannelCallback &cbk) { _onLostConnectionCbk = cbk; }
        void lostConnectionCbk() {
            if (_onLostConnectionCbk) _onLostConnectionCbk(shared_from_this());
        }
        void setReadyReadCbk(const CommChannelCallback &cbk) { _onReadyReadCbk = cbk; }
        void readyReadCbk() {
            if (_onReadyReadCbk) _onReadyReadCbk(shared_from_this());
        }
        void setDestroyedCbk(const CommChannelCallback &cbk) { _onDestroyedCbk = cbk; }
        void destroyedCbk() {
            if (_onDestroyedCbk) _onDestroyedCbk(shared_from_this());
        }

    private:
        CommString _readBuffer;
        CommChannelCallback _onLostConnectionCbk;
        CommChannelCallback _onReadyReadCbk;
        CommChannelCallback _onDestroyedCbk;

        //! Reads from the device.
        /*!
          \param data is a char array pointer.
          \param maxSize is is the maximum number of bytes to read.
          \return the number of bytes read or -1 if an error occurred.
        */
        virtual uint64_t readData(CommChar *data, uint64_t maxSize) = 0;

        //! Writes to the device.
        /*!
          \param data is a char array pointer.
          \param maxSize is is the maximum number of bytes to write.
          \return the number of bytes written or -1 if an error occurred.
        */
        virtual uint64_t writeData(const CommChar *data, uint64_t maxSize) = 0;

        CommString truncateLongLogMessage(const CommString &message);
};

} // namespace KDC
