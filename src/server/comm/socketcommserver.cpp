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

#include "socketcommserver.h"

namespace KDC {

SocketCommChannel::SocketCommChannel() :
    AbstractCommChannel() {}

SocketCommChannel::~SocketCommChannel() {}

uint64_t SocketCommChannel::readData(char *data, uint64_t maxlen) {
    return 0;
}

uint64_t SocketCommChannel::writeData(const char *data, uint64_t len) {
    return 0;
}

uint64_t SocketCommChannel::bytesAvailable() const {
    return 0;
}

SocketCommServer::SocketCommServer(const std::string &name) :
    AbstractCommServer(name) {}

SocketCommServer::~SocketCommServer() {}

void SocketCommServer::close() {}

bool SocketCommServer::listen(const KDC::SyncPath &) {
    return true;
}

std::shared_ptr<AbstractCommChannel> SocketCommServer::nextPendingConnection() {
    return nullptr;
}

std::list<std::shared_ptr<AbstractCommChannel>> SocketCommServer::connections() {
    return std::list<std::shared_ptr<AbstractCommChannel>>();
}

} // namespace KDC
