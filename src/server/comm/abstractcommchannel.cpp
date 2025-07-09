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

#include "abstractcommchannel.h"
#include "libcommon/utility/types.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/utility/utility.h"

namespace KDC {

AbstractCommChannel::~AbstractCommChannel() {
    destroyedCbk();
}

bool AbstractCommChannel::open() {
    return true;
}

void AbstractCommChannel::close() {
    _readBuffer.clear();
}

void AbstractCommChannel::sendMessage(const CommString &message, bool doWait) {
    const CommString truncatedLogMessage = truncateLongLogMessage(message);
    LOGW_INFO(KDC::Log::instance()->getLogger(),
              L"Sending message: " << CommString2WStr(truncatedLogMessage) << L" to: " << this);

    CommString localMessage = message;
    if (!localMessage.ends_with('\n')) {
        localMessage.append("\n");
    }

    auto sent = write(localMessage);
    if (doWait) {
        waitForBytesWritten(1000);
    }
    if (sent != localMessage.size()) {
        LOGW_WARN(KDC::Log::instance()->getLogger(), L"Could not send all data on socket for " << CommString2WStr(localMessage));
    }
}

uint64_t AbstractCommChannel::write(const CommString &data) {
    std::string dataStr = CommString2Str(data);
    return writeData(dataStr.c_str(), dataStr.length());
}

bool AbstractCommChannel::waitForBytesWritten(int msecs) {
    (void) msecs;
    return true;
}

CommString AbstractCommChannel::readLine() {
    static const uint64_t maxlen = 1024;
    char data[maxlen];
    uint64_t readSize = 0;
    CommString line;
    do {
        readSize = readData(data, maxlen);
        _readBuffer += Str2CommString(data);
        if (auto eofPos = _readBuffer.find('\n'); eofPos != std::string::npos) {
            line = _readBuffer.substr(0, eofPos);
            _readBuffer.erase(0, eofPos);
            break;
        }
    } while (readSize == maxlen);
    return line;
}

CommString AbstractCommChannel::truncateLongLogMessage(const CommString &message) {
    if (static const size_t maxLogMessageSize = 2048; message.size() > maxLogMessageSize) {
        return message.substr(0, maxLogMessageSize) + " (truncated)";
    }

    return message;
}

} // namespace KDC
