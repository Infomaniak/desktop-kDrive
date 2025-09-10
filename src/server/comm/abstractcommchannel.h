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

#define FINDER_EXT_LINE_SEPARATOR Str("\n")
#define FINDER_EXT_QUERY_SEPARATOR Str("\\/")
#define GUI_ARG_SEPARATOR Str(";")

namespace KDC {

class AbstractCommChannel : public std::enable_shared_from_this<AbstractCommChannel> {
    public:
        AbstractCommChannel() = default;
        virtual ~AbstractCommChannel();

        virtual void close() = 0;
        virtual void sendMessage(const CommString &message) = 0;
        virtual bool canReadMessage() const = 0;
        virtual CommString readMessage() = 0;
        virtual uint64_t bytesAvailable() const = 0;

        //! Gets an unique identifier for the object.
        /*!
          \return the object ptr casted to string.
        */
        std::string id();

        // Callbacks
        void setLostConnectionCbk(const std::function<void(std::shared_ptr<AbstractCommChannel>)> &cbk) {
            _onLostConnectionCbk = cbk;
        }
        void setReadyReadCbk(const std::function<void(std::shared_ptr<AbstractCommChannel>)> &cbk) { _onReadyReadCbk = cbk; }

        void lostConnectionCbk() {
            auto thisPtr = weak_from_this().lock(); // Ensure the callback is not called on an object being destroyed
            if (_onLostConnectionCbk && thisPtr) _onLostConnectionCbk(thisPtr);
        }
        void readyReadCbk() {
            auto thisPtr = weak_from_this().lock(); // Ensure the callback is not called on an object being destroyed
            if (_onReadyReadCbk && thisPtr) _onReadyReadCbk(thisPtr);
        }

    protected:
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


    private:
        std::function<void(std::shared_ptr<AbstractCommChannel>)> _onLostConnectionCbk;
        std::function<void(std::shared_ptr<AbstractCommChannel>)> _onReadyReadCbk;
        std::function<void(std::shared_ptr<AbstractCommChannel>)> _onDestroyedCbk;

        friend class TestSocketComm;
};

} // namespace KDC
