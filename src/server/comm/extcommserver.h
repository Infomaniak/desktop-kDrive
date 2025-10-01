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
#include "pipecommserver.h"
#endif

namespace KDC {
static const auto finderExtLineSeparator = Str("\n");
static const auto finderExtQuerySeparator = Str("\\/");

#if defined(KD_MACOS)
class ExtCommChannel : public XPCCommChannel {
#else
class ExtCommChannel : public PipeCommChannel {
#endif

    public:
#if defined(KD_MACOS)
        using XPCCommChannel::XPCCommChannel;
#else
        using PipeCommChannel::PipeCommChannel;
#endif
        bool sendMessage(const CommString &message) final;
        bool canReadMessage() final;
        CommString readMessage() final;

    private:
#if defined(KD_MACOS)
        uint64_t writeData(const KDC::CommChar *data, uint64_t len) override;
#endif
        CommString _readBuffer;
};

#if defined(KD_MACOS)
class ExtCommServer : public XPCCommServer {
#else
class ExtCommServer : public PipeCommServer {
#endif

    public:
#if defined(KD_MACOS)
        ExtCommServer(const std::string &name);
#else
        using PipeCommServer::PipeCommServer;
        std::shared_ptr<PipeCommChannel> makeCommChannel() const override { return std::make_shared<ExtCommChannel>(); }
#endif
};
} // namespace KDC
