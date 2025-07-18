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

#include "abstractcommserver.h"
#include "libcommon/utility/types.h"
#include "libcommonserver/vfs/vfs.h"
#include "libsyncengine/syncpal/syncpal.h"

#include <unordered_map>

namespace KDC {

class ExtensionJob;

class CommManager : public std::enable_shared_from_this<CommManager> {
    public:
        explicit CommManager(const std::unordered_map<int, std::shared_ptr<SyncPal>> &syncPalMap,
                             const std::unordered_map<int, std::shared_ptr<Vfs>> &vfsMap);
        ~CommManager();

        // AppServer callbacks setting
        inline void setAddErrorCallback(void (*addError)(const Error &)) { _addError = addError; }
        inline void setGetThumbnailCallback(ExitCode (*getThumbnail)(int, NodeId, int, std::string &)) {
            _getThumbnail = getThumbnail;
        }
        inline void setGetPublicLinkUrlCallback(ExitCode (*getPublicLinkUrl)(int, const NodeId &, std::string &)) {
            _getPublicLinkUrl = getPublicLinkUrl;
        }

        // Register a new sync path on all the Finder extensions
        void registerSync(const SyncPath &localPath);
        // Unregister a sync path on all the Finder extensions
        void unregisterSync(const SyncPath &localPath);
        // TODO: to remove when the LiteSync server will be implemented
        void executeCommandDirect(const CommString &commandLineStr, bool broadcast);

    private:
        // AppServer maps
        const std::unordered_map<int, std::shared_ptr<SyncPal>> &_syncPalMap;
        const std::unordered_map<int, std::shared_ptr<Vfs>> &_vfsMap;

        // AppServer callbacks
        void (*_addError)(const Error &error);
        ExitCode (*_getThumbnail)(int driveDbId, NodeId nodeId, int width, std::string &thumbnail);
        ExitCode (*_getPublicLinkUrl)(int driveDbId, const NodeId &nodeId, std::string &linkUrl);

        // Communication server
        std::unique_ptr<AbstractCommServer> _commServer;

        // Execute a command received from an extension, which does not require an answer
        void executeCommand(const CommString &commandLineStr);
        // Execute a command received from an extension and responds on the provided channel
        void executeCommand(const CommString &commandLineStr, std::shared_ptr<AbstractCommChannel> channel);
        // Broadcast a command to all the server channels
        void broadcastCommand(const CommString &commandLineStr);

        // Finder callbacks
        void onNewExtConnection(); // Can be a Mac Finder Extension or a Windows Shell Extension
        void onExtQueryReceived(std::shared_ptr<AbstractCommChannel> channel);
        void onLostExtConnection(std::shared_ptr<AbstractCommChannel> channel);

        // GUI callbacks
        void onNewGuiConnection();
        void onGuiQueryReceived(std::shared_ptr<AbstractCommChannel> channel);
        void onLostGuiConnection(std::shared_ptr<AbstractCommChannel> channel);

#if defined(_WIN32) or defined(__unix__)
        // Windows & Linux: socket interprocess communication
        SyncPath createSocket();
#else
        // macOS: XPC interprocess communication
#endif

        friend class ExtensionJob;
};

} // namespace KDC
