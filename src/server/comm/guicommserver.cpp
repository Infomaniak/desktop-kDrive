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

#include "guicommserver.h"
#include <Poco/JSON/Parser.h>
namespace KDC {

bool GuiCommChannel::sendMessage(const CommString &message) {
    const CommString truncatedLogMessage = truncateLongLogMessage(message);
    LOGW_INFO(Log::instance()->getLogger(), L"Sending message: " << CommonUtility::commString2WStr(truncatedLogMessage)
                                                                 << L" to: " << CommonUtility::s2ws(id()));

    // Check that the message is a valid JSON
    Poco::JSON::Parser parser;
    try {
        parser.parse(SyncName2Str(message));
    } catch (const Poco::Exception &e) {
        LOGW_ERROR(Log::instance()->getLogger(), L"Failed to send message to GUI, invalid JSON: "
                                                         << CommonUtility::commString2WStr(message) << L", error: "
                                                         << CommonUtility::s2ws(e.displayText()));
        return false;
    }
    (void) writeData(message.c_str(), message.size());
    return true;
}

bool GuiCommChannel::canReadMessage() {
    fetchDataToBuffer();
    _validJsonInBuffer = containsValidJson(_readBuffer, _inBufferJsonEndIndex);
    return _validJsonInBuffer;
}

CommString GuiCommChannel::readMessage() {
    if (!_validJsonInBuffer) {
        fetchDataToBuffer();
        _validJsonInBuffer = containsValidJson(_readBuffer, _inBufferJsonEndIndex);
    }
    if (!_validJsonInBuffer) {
        return Str("");
    }

    CommString message = _readBuffer.substr(0, _inBufferJsonEndIndex + 1);
    _readBuffer.erase(0, _inBufferJsonEndIndex + 1);
    const CommString truncatedLogMessage = truncateLongLogMessage(message);
    LOGW_INFO(Log::instance()->getLogger(), L"Reading message: " << CommonUtility::commString2WStr(truncatedLogMessage)
                                                                 << L" from: " << CommonUtility::s2ws(id()));

    if (canReadMessage()) {
        LOG_INFO(Log::instance()->getLogger(), "More messages available in buffer, trigger another read");
        readyReadCbk();
    }
    return message;
}

bool GuiCommChannel::containsValidJson(const CommString &message, int &endIndex) const {
    endIndex = -1;
    if (message.empty() || (message[0] != '{' && message[0] != '[')) {
        return false;
    }

    // Simple check for matching braces/brackets
    CommChar openChar = message[0];
    CommChar closeChar = (openChar == '{') ? '}' : ']';
    int balance = 0;
    for (size_t i = 0; i < message.size(); ++i) {
        if (message[i] == openChar) {
            balance++;
        } else if (message[i] == closeChar) {
            balance--;
            if (balance == 0) {
                endIndex = static_cast<int>(i);
                return true;
            }
        }
    }

    return false;
}

void GuiCommChannel::fetchDataToBuffer() {
    while (bytesAvailable() > 0) {
        CommChar data[1024];
        if (uint64_t charRead = readData(data, 1024); charRead > 0) {
            _readBuffer.append(data, charRead);
        } else {
            break;
        }
    }
}

} // namespace KDC
