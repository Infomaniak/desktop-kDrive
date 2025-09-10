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

#include "xpccommserver_mac.h"
#include "libcommon/utility/types.h"

#include <string>

class GuiCommServerPrivate;
class GuiCommChannelPrivate;

class GuiCommChannel : public XPCCommChannel {
    public:
        GuiCommChannel(GuiCommChannelPrivate *p);
        void sendMessage(const KDC::CommString &message) final { /* TODO: Implement */ }
        bool canReadMessage() const final { return false; /* TODO: Implement */ }
        KDC::CommString readMessage() final { return Str(""); /*TODO: Implement */ }

    private:
        uint64_t writeData(const KDC::CommChar *data, uint64_t len) override;
};

class GuiCommServer : public XPCCommServer {
    public:
        GuiCommServer(const std::string &name);
};
