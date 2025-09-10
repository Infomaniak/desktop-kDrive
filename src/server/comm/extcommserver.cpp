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

#include "extcommserver.h"

namespace KDC {

ExtCommChannel::ExtCommChannel() :
    PipeCommChannel() {}

bool ExtCommChannel::sendMessage(const CommString &message) {
    const CommString truncatedLogMessage = truncateLongLogMessage(message);
    LOGW_INFO(Log::instance()->getLogger(), L"Sending message: " << CommonUtility::commString2WStr(truncatedLogMessage)
                                                                 << L" to: " << CommonUtility::s2ws(id()));

    // Add messages separator if needed
    CommString localMessage = message;
    if (!localMessage.ends_with(finderExtLineSeparator)) {
        localMessage += finderExtLineSeparator;
    }

    if (auto sent = writeData(localMessage.c_str(), localMessage.length()); !sent) {
        LOG_WARN(Log::instance()->getLogger(), "Error in AbstractCommChannel::writeData");
        return false;
    }
    return true;
}

CommString ExtCommChannel::readMessage() {
    static const uint64_t maxLineLength = 1024; // Allows you to read most lines at once
    CommChar data[maxLineLength];
    CommString line;
    forever {
        if (auto sepPos = _readBuffer.find(finderExtLineSeparator); sepPos != std::string::npos) {
            line = _readBuffer.substr(0, sepPos);
            _readBuffer.erase(0, sepPos + 1);
            break;
        }
        if (auto readSize = readData(data, maxLineLength); readSize > 0) {
            CommString dataStr(data, readSize);
            _readBuffer.append(dataStr);
        } else {
            break;
        }
    }
    return line;
}

bool ExtCommChannel::canReadMessage() const {
    return _readBuffer.find(finderExtLineSeparator, 0) != std::string::npos;
}
ExtCommServer::ExtCommServer(const std::string &name) :
    PipeCommServer(name) {}

} // namespace KDC
