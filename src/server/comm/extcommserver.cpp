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
    CommString query;
    forever {
        if (auto sepPos = _readBuffer.find(finderExtQuerySeparator); sepPos != std::string::npos) {
            query = _readBuffer.substr(0, sepPos);
            _readBuffer.erase(0, sepPos + CommonUtility::strLen(finderExtQuerySeparator));
            return query;
        }
        if (auto readSize = readData(data, maxLineLength); readSize > 0) {
            CommString dataStr(data, readSize);
            _readBuffer.append(dataStr);
        } else {
            break;
        }
    }
    return CommString{};
}

bool ExtCommChannel::canReadMessage() {
    while (bytesAvailable() > 0) {
        CommChar data[1024];
        if (uint64_t charRead = readData(data, 1024); charRead > 0) {
            _readBuffer.append(data, charRead);
        } else {
            break;
        }
    }
    return _readBuffer.find(finderExtQuerySeparator) != std::string::npos;
}
} // namespace KDC
