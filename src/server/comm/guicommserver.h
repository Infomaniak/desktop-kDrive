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

#if defined(KD_MACOS)
#include "xpccommserver_mac.h"
#else
#include "socketcommserver.h"
#endif

#include <string>

namespace KDC {
#if defined(KD_MACOS)
class GuiCommChannel : public XPCCommChannel {
    public:
        using XPCCommChannel::XPCCommChannel;
#else
class GuiCommChannel : public SocketCommChannel {
    public:
        using SocketCommChannel::SocketCommChannel;
#endif
        // Gui channel expect JSON messages only
        bool sendMessage(const CommString &message) final;
        bool canReadMessage() final;
        CommString readMessage() final;

#if defined(KD_MACOS)
        // Run sendQuery xpc method - For tests only
        static void runSendQuery(const CommString &query,
                                 const std::function<void(std::shared_ptr<AbstractCommChannel>)> &readyReadCbk,
                                 const std::function<void(const CommString &answer)> &answerCbk);
#endif

    private:
#if defined(KD_MACOS)
        uint64_t writeData(const KDC::CommChar *data, uint64_t len) override;
#endif
        bool containsValidJson(const CommString &message, size_t &endIndex) const;
        void fetchDataToBuffer();

        CommString _readBuffer;
        bool _validJsonInBuffer = false; // True if read buffer contains at least one valid JSON
        size_t _inBufferJsonEndIndex = 0; // Index of the end of the valid JSON in the buffer
};

#if defined(KD_MACOS)
class GuiCommServer : public XPCCommServer {
    public:
        GuiCommServer(const std::string &name);
#else
class GuiCommServer : public SocketCommServer {
    public:
        using SocketCommServer::SocketCommServer;
        std::shared_ptr<SocketCommChannel> makeCommChannel(Poco::Net::StreamSocket &socket) const override {
            return std::make_shared<GuiCommChannel>(socket);
        }
#endif
};
} // namespace KDC
