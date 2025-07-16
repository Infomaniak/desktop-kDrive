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

#include "abstractcommchannel.h"
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
        virtual ~CommManager();

        // AppServer callbacks setting
        inline void setAddErrorCallback(void (*addError)(const Error &)) { _addError = addError; }
        inline void setGetThumbnailCallback(ExitCode (*getThumbnail)(int, NodeId, int, std::string &)) {
            _getThumbnail = getThumbnail;
        }
        inline void setGetPublicLinkUrlCallback(ExitCode (*getPublicLinkUrl)(int, const NodeId &, std::string &)) {
            _getPublicLinkUrl = getPublicLinkUrl;
        }

        void executeCommandDirect(const CommString &commandLineStr, bool broadcast);

    private:
        // AppServer maps
        const std::unordered_map<int, std::shared_ptr<SyncPal>> &_syncPalMap;
        const std::unordered_map<int, std::shared_ptr<Vfs>> &_vfsMap;

        std::unique_ptr<AbstractCommServer> _commServer;

        // AppServer callbacks
        void (*_addError)(const Error &error);
        ExitCode (*_getThumbnail)(int driveDbId, NodeId nodeId, int width, std::string &thumbnail);
        ExitCode (*_getPublicLinkUrl)(int driveDbId, const NodeId &nodeId, std::string &linkUrl);

        std::unordered_set<int> _registeredSyncs;

        // Callbacks
        void onNewExtConnection(); // Can be a Mac Finder Extension or a Windows Shell Extension
        void onExtQueryReceived(std::shared_ptr<AbstractCommChannel> channel);
        void onLostExtConnection(std::shared_ptr<AbstractCommChannel> channel);

        void onNewGuiConnection();
        void onGuiQueryReceived(std::shared_ptr<AbstractCommChannel> channel);
        void onLostGuiConnection(std::shared_ptr<AbstractCommChannel> channel);

        void executeCommand(const CommString &commandLineStr);
        void executeCommand(const CommString &commandLineStr, std::shared_ptr<AbstractCommChannel> channel);
        void broadcastCommand(const CommString &commandLineStr);

        // Try to retrieve the Sync object with DB ID `syncDbId`.
        // Returns `false`, add errors and log messages on failure.
        // Returns `true` and set `sync` with the result otherwise.
        bool tryToRetrieveSync(const int syncDbId, Sync &sync) const;

        void registerSync(int syncDbId);
        void unregisterSync(int syncDbId);

        SyncPath createSocket();


        friend class ExtensionJob;
};

} // namespace KDC
