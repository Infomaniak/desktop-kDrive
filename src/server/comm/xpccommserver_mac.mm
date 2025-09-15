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

#include "xpccommserver_mac.h"
#include "abstractcommserver_mac.h"

// XPCCommChannel implementation
XPCCommChannel::XPCCommChannel(AbstractCommChannelPrivate *p) :
    _privatePtr(p) {
    _privatePtr->publicPtr = this;
}

XPCCommChannel::~XPCCommChannel() {}

uint64_t XPCCommChannel::readData(KDC::CommChar *data, uint64_t maxSize) {
    auto size = _privatePtr->inBuffer.copy(data, maxSize);
    _privatePtr->inBuffer.erase(0, size);
    return size;
}

uint64_t XPCCommChannel::bytesAvailable() const {
    return _privatePtr->inBuffer.size();
}

// XPCCommServer implementation
XPCCommServer::XPCCommServer(const std::string &name, AbstractCommServerPrivate *commServerPrivate) :
    KDC::AbstractCommServer(name),
    _privatePtr(commServerPrivate) {
    _privatePtr->publicPtr = this;
}

XPCCommServer::~XPCCommServer() {}

void XPCCommServer::close() {}

bool XPCCommServer::listen(const KDC::SyncPath &) {
    [_privatePtr->server start];
    return true;
}

std::shared_ptr<KDC::AbstractCommChannel> XPCCommServer::nextPendingConnection() {
    return _privatePtr->pendingChannels.back();
}

std::list<std::shared_ptr<KDC::AbstractCommChannel>> XPCCommServer::connections() {
    std::list<std::shared_ptr<KDC::AbstractCommChannel>> res(_privatePtr->pendingChannels.begin(),
                                                             _privatePtr->pendingChannels.end());
    return res;
}
