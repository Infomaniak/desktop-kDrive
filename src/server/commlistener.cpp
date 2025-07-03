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

CommListener::CommListener(QIODevice *ioDevice) : ioDevice(ioDevice) {
    _threadId = std::this_thread::get_id();
}


void CommListener::sendMessage(const QString &message, bool doWait) const {
    assert(_threadId == std::this_thread::get_id() && "CommListener::sendMessage should only be called from the main thread");

    if (!ioDevice) {
        LOGW_INFO(KDC::Log::instance()->getLogger(), L"Not sending message to dead ioDevice: " << message.toStdWString());
        return;
    }

    const QString truncatedLogMessage = CommonUtility::truncateLongLogMessage(message);

    LOGW_INFO(KDC::Log::instance()->getLogger(),
              L"Sending message: " << truncatedLogMessage.toStdWString() << L" to: " << ioDevice);

    QString localMessage = message;
    if (!localMessage.endsWith(QLatin1Char('\n'))) {
        localMessage.append(QLatin1Char('\n'));
    }

    QByteArray bytesToSend = localMessage.toUtf8();
    qint64 sent = ioDevice->write(bytesToSend);
    if (doWait) {
        ioDevice->waitForBytesWritten(1000);
    }
    if (sent != bytesToSend.length()) {
        LOGW_WARN(KDC::Log::instance()->getLogger(), L"Could not send all data on ioDevice for " << localMessage.toStdWString());
    }
}

void CommListener::sendMessageIfDirectoryMonitored(const QString &message, uint systemDirectoryHash) const {
    if (_monitoredDirectoriesBloomFilter.isHashMaybeStored(systemDirectoryHash)) sendMessage(message, false);
}

void CommListener::registerMonitoredDirectory(uint systemDirectoryHash) {
    _monitoredDirectoriesBloomFilter.storeHash(systemDirectoryHash);
}

} // namespace KDC
