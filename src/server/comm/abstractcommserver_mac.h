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
#include "abstractcommserver.h"
#include "../libcommon/utility/utility.h"
#import "../../extensions/MacOSX/kDriveFinderSync/kDrive/NSXPCConnection+LoginItem.h"
#include "../../extensions/MacOSX/kDriveFinderSync/LoginItemAgent/xpcLoginItemProtocol.h"

class AbstractCommChannelPrivate;
class AbstractCommServerPrivate;

@interface AbstractLocalEnd : NSObject

@property AbstractCommChannelPrivate *wrapper;

- (instancetype)initWithWrapper:(AbstractCommChannelPrivate *)wrapper;

@end


@interface AbstractRemoteEnd : NSObject

@property(retain) NSXPCConnection *connection;

- (id)init:(NSXPCConnection *)connection;

@end


@interface AbstractServer : NSObject <NSXPCListenerDelegate, XPCLoginItemRemoteProtocol>

@property AbstractCommServerPrivate *wrapper;
@property(retain) NSXPCListener *listener;
@property(retain) NSXPCConnection *loginItemAgentConnection;

- (instancetype)initWithWrapper:(AbstractCommServerPrivate *)wrapper;
- (void)start;
- (void)dealloc;

- (void)setServerEndpoint:(NSXPCListenerEndpoint *)endpoint;

@end

class AbstractCommChannelPrivate {
    public:
        KDC::AbstractCommChannel *publicPtr;

        AbstractCommChannelPrivate(NSXPCConnection *remoteConnection);
        ~AbstractCommChannelPrivate();

        // release remoteEnd
        void disconnectRemote();

        AbstractRemoteEnd *_remoteEnd;
        AbstractLocalEnd *_localEnd;
        KDC::CommString _inBuffer;
        bool _isRemoteDisconnected = false;
};

class AbstractCommServerPrivate {
    public:
        KDC::AbstractCommServer *publicPtr;

        AbstractCommServerPrivate();
        ~AbstractCommServerPrivate();

        std::list<std::shared_ptr<KDC::AbstractCommChannel>> _pendingChannels;
        AbstractServer *_server;
};
