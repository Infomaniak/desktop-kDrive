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

#include "commlistener.h"
#include "libcommonserver/log/log.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/utility/utility.h"

#include <log4cplus/loggingmacros.h>

namespace KDC {

BloomFilter::BloomFilter() :
    _hashBits(NumBits) {}

void BloomFilter::storeHash(uint hash) {
    _hashBits.setBit((hash & 0xFFFF) % NumBits);
    _hashBits.setBit((hash >> 16) % NumBits);
}

bool BloomFilter::isHashMaybeStored(uint hash) const {
    return _hashBits.testBit((hash & 0xFFFF) % NumBits) && _hashBits.testBit((hash >> 16) % NumBits);
}

CommListener::CommListener(AbstractIODevice *ioDevice) :
    ioDevice(ioDevice) {
    _threadId = std::this_thread::get_id();
}


void CommListener::sendMessage(const CommString &message, bool doWait) const {
    assert(_threadId == std::this_thread::get_id() && "CommListener::sendMessage should only be called from the main thread");

    if (!ioDevice) {
        LOGW_INFO(Log::instance()->getLogger(), L"Do not send message to dead ioDevice: " << CommString2WStr(message));
        return;
    }

    const CommString truncatedLogMessage = truncateLongLogMessage(message);
    LOGW_INFO(Log::instance()->getLogger(),
              L"Sending message: " << CommString2WStr(truncatedLogMessage) << L" to: " << ioDevice.get());

    CommString localMessage = message;
    if (!localMessage.ends_with('\n')) {
        localMessage.append("\n");
    }

    uint64_t sent = ioDevice->write(localMessage);
    if (doWait) {
        ioDevice->waitForBytesWritten(1000);
    }
    if (sent != localMessage.size()) {
        LOGW_WARN(Log::instance()->getLogger(), L"Could not send all data on ioDevice for " << CommString2WStr(localMessage));
    }
}

void CommListener::sendMessageIfDirectoryMonitored(const CommString &message, uint systemDirectoryHash) const {
    if (_monitoredDirectoriesBloomFilter.isHashMaybeStored(systemDirectoryHash)) sendMessage(message, false);
}

void CommListener::registerMonitoredDirectory(uint systemDirectoryHash) {
    _monitoredDirectoriesBloomFilter.storeHash(systemDirectoryHash);
}

CommString CommListener::truncateLongLogMessage(const CommString &message) {
    if (static const size_t maxLogMessageSize = 2048; message.size() > maxLogMessageSize) {
        return message.substr(0, maxLogMessageSize) + " (truncated)";
    }

    return message;
}

} // namespace KDC
