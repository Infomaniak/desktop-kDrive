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

namespace KDC {

GuiCommChannel::GuiCommChannel() :
    SocketCommChannel() {}

void GuiCommChannel::sendMessage(const CommString &message) {
    const CommString truncatedLogMessage = truncateLongLogMessage(message);
    LOGW_INFO(Log::instance()->getLogger(), L"Sending message: " << CommonUtility::commString2WStr(truncatedLogMessage)
                                                                 << L" to: " << CommonUtility::s2ws(id()));
    // TODO: Format message for GUI (e.g. add line separator)
}

CommString GuiCommChannel::readMessage() {
    // TODO: Read message formatted for GUI (e.g. until line separator)
    return Str("");
}

bool GuiCommChannel::canReadMessage() const {
    // TODO: Implement message availability check for GUI
    return false;
}

GuiCommServer::GuiCommServer(const std::string &name) :
    SocketCommServer(name) {}

} // namespace KDC
