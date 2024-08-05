/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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

#include "socketlistener.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/utility/utility.h"
#include <log4cplus/loggingmacros.h>

namespace KDC {

BloomFilter::BloomFilter() : hashBits(NumBits) {}

void BloomFilter::storeHash(uint hash) {
    hashBits.setBit((hash & 0xFFFF) % NumBits);
    hashBits.setBit((hash >> 16) % NumBits);
}

bool BloomFilter::isHashMaybeStored(uint hash) const {
    return hashBits.testBit((hash & 0xFFFF) % NumBits) && hashBits.testBit((hash >> 16) % NumBits);
}

SocketListener::SocketListener(QIODevice *socket) : socket(socket) {
    _threadId = std::this_thread::get_id();
}

void SocketListener::sendMessage(const QString &message, bool doWait) const {
    if (_threadId != std::this_thread::get_id()) {
        LOGW_ERROR(KDC::Log::instance()->getLogger(), L"SocketListener::sendMessage should only be called from the main thread");
        assert(false);
    }

    if (!socket) {
        LOGW_INFO(KDC::Log::instance()->getLogger(), L"Not sending message to dead socket: " << message.toStdWString().c_str());
        return;
    }

    LOGW_INFO(KDC::Log::instance()->getLogger(),
              L"Sending SocketAPI message --> " << message.toStdWString().c_str() << L" to " << socket);
    QString localMessage = message;
    if (!localMessage.endsWith(QLatin1Char('\n'))) {
        localMessage.append(QLatin1Char('\n'));
    }

    QByteArray bytesToSend = localMessage.toUtf8();
    qint64 sent = socket->write(bytesToSend);
    if (doWait) {
        socket->waitForBytesWritten(1000);
    }
    if (sent != bytesToSend.length()) {
        LOGW_WARN(KDC::Log::instance()->getLogger(),
                  L"Could not send all data on socket for " << localMessage.toStdWString().c_str());
    }
}

void SocketListener::sendMessageIfDirectoryMonitored(const QString &message, uint systemDirectoryHash) const {
    if (_monitoredDirectoriesBloomFilter.isHashMaybeStored(systemDirectoryHash)) sendMessage(message, false);
}

void SocketListener::registerMonitoredDirectory(uint systemDirectoryHash) {
    _monitoredDirectoriesBloomFilter.storeHash(systemDirectoryHash);
}

}  // namespace KDC
