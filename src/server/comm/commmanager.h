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

class GuiJobFactory;

class CommManager : public std::enable_shared_from_this<CommManager> {
    public:
        explicit CommManager(const SyncPalMap &syncPalMap, const VfsMap &vfsMap);
        ~CommManager();

        void start();
        void stop();

        // AppServer maps
        const SyncPalMap &syncPalMap() const { return _syncPalMap; }
        const VfsMap &vfsMap() const { return _vfsMap; }

        // AppServer callbacks
        inline void setAddErrorCbk(const std::function<void(const Error &)> &addErrorCbk) { _addErrorCbk = addErrorCbk; }
        inline void addErrorCbk(const Error &error) const {
            if (_addErrorCbk) _addErrorCbk(error);
        }

        inline void setUpdateSentryUserCbk(const std::function<void()> &updateSentryUserCbk) {
            _updateSentryUserCbk = updateSentryUserCbk;
        }
        inline void updateSentryUserCbk() const {
            if (_updateSentryUserCbk) _updateSentryUserCbk();
        }

#if defined(KD_MACOS) || defined(KD_WINDOWS)
        inline void setGetThumbnailCbk(const std::function<ExitCode(int, const NodeId &, int, std::string &)> &getThumbnailCbk) {
            _getThumbnailCbk = getThumbnailCbk;
        }
        inline ExitCode getThumbnailCbk(int driveDbId, const NodeId &nodeId, int width, std::string &thumbnail) const {
            if (_getThumbnailCbk)
                return _getThumbnailCbk(driveDbId, nodeId, width, thumbnail);
            else
                return ExitCode::LogicError;
        }

        inline void setGetPublicLinkUrlCbk(
                const std::function<ExitCode(int, const NodeId &, std::string &)> &getPublicLinkUrlCbk) {
            _getPublicLinkUrlCbk = getPublicLinkUrlCbk;
        }
        inline ExitCode getPublicLinkUrlCbk(int driveDbId, const NodeId &nodeId, std::string &linkUrl) const {
            if (_getPublicLinkUrlCbk)
                return _getPublicLinkUrlCbk(driveDbId, nodeId, linkUrl);
            else
                return ExitCode::LogicError;
        }

        // Register a new sync path on all the Finder/File Explorer channels
        // macOS: Multiple copies of the Finder extension can be running at the same time
        // cf. https://developer.apple.com/library/archive/documentation/General/Conceptual/ExtensibilityPG/Finder.html
        // Windows: KDOverlays & KDContextMenu each use a channel
        void registerSync(const SyncPath &localPath);
        // Unregister a sync path on all the Finder/File Explorer channels
        void unregisterSync(const SyncPath &localPath);
        // TODO: to remove when the LiteSync client will be implemented
        void executeCommandDirect(const CommString &commandLineStr, bool broadcast);
#endif

    private:
        // AppServer maps
        const SyncPalMap &_syncPalMap;
        const VfsMap &_vfsMap;

        // AppServer callbacks
        std::function<void(const Error &)> _addErrorCbk;
        std::function<void()> _updateSentryUserCbk;
#if defined(KD_MACOS) || defined(KD_WINDOWS)
        std::function<ExitCode(int, const NodeId &, int, std::string &)> _getThumbnailCbk;
        std::function<ExitCode(int, const NodeId &, std::string &)> _getPublicLinkUrlCbk;
#endif

        // Communication servers
#if defined(KD_MACOS) || defined(KD_WINDOWS)
        std::shared_ptr<AbstractCommServer> _extCommServer;
#endif
        std::shared_ptr<AbstractCommServer> _guiCommServer;

        std::unique_ptr<GuiJobFactory> _guiJobFactory;

#if defined(KD_MACOS) || defined(KD_WINDOWS)
        // Execute a command received from an extension, which does not require an answer
        void executeExtCommand(const CommString &commandLineStr);
        // Execute a command received from an extension and responds on the provided channel
        void executeExtCommand(const CommString &commandLineStr, std::shared_ptr<AbstractCommChannel> channel);
        // Broadcast a command to all the server channels
        void broadcastExtCommand(const CommString &commandLineStr);

        // Finder callbacks
        void onNewExtConnection(); // Can be a Mac Finder Extension or a Windows Shell Extension
        void onExtQueryReceived(std::shared_ptr<AbstractCommChannel> channel);
        void onLostExtConnection(std::shared_ptr<AbstractCommChannel> channel);
#endif
        // Execute a query received from the GUI and responds on the provided channel
        void executeGuiQuery(const CommString &commandLineStr, std::shared_ptr<AbstractCommChannel> channel);

        // GUI callbacks
        void onNewGuiConnection();
        void onGuiQueryReceived(std::shared_ptr<AbstractCommChannel> channel);
        void onLostGuiConnection(std::shared_ptr<AbstractCommChannel> channel);
};

} // namespace KDC
