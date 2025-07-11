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

struct FileData {
        FileData() = default;

        static FileData get(const SyncPath &path);
        FileData parentFolder() const;

        // Absolute path of the file locally
        SyncPath localPath;

        // Relative path of the file
        SyncPath relativePath;

        int syncDbId{0};
        int driveDbId{0};

        bool isDirectory{false};
        bool isLink{false};
        VirtualFileMode virtualFileMode{VirtualFileMode::Off};
};

class CommManager {
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

        // Finder/File explorer actions
        void registerSync(int syncDbId);
        void unregisterSync(int syncDbId);

        void executeCommandDirect(const CommString &commandLine);

    private:
        const std::unordered_map<int, std::shared_ptr<SyncPal>> &_syncPalMap;
        const std::unordered_map<int, std::shared_ptr<Vfs>> &_vfsMap;

        // Extensions commands map
        std::map<std::string, std::function<void(const CommString &, std::shared_ptr<AbstractCommChannel>)>> _commands;

        std::unique_ptr<AbstractCommServer> _localServer;

        std::unordered_set<int> _registeredSyncs;

        bool _dehydrationCanceled = false;
        unsigned _nbOngoingDehydration = 0;
        std::mutex _dehydrationMutex;

        // Callbacks
        void onNewExtConnection(); // Can be a Mac Finder Extension or a Windows Shell Extension
        void onExtQueryReceived(std::shared_ptr<AbstractCommChannel> channel);
        void onLostExtConnection(std::shared_ptr<AbstractCommChannel> channel);

        void onNewGuiConnection();
        void onGuiQueryReceived(std::shared_ptr<AbstractCommChannel> channel);
        void onLostGuiConnection(std::shared_ptr<AbstractCommChannel> channel);

        // AppServer callbacks
        void (*_addError)(const Error &error);
        ExitCode (*_getThumbnail)(int driveDbId, NodeId nodeId, int width, std::string &thumbnail);
        ExitCode (*_getPublicLinkUrl)(int driveDbId, const NodeId &nodeId, std::string &linkUrl);

        // Send message to all extension channels
        void broadcastMessage(const CommString &msg, bool doWait = false);

        /** Execute commands requested by the extensions
         * macOS:
         *  - FinderSyncExt
         *  - LiteSyncExt
         * Windows:
         *  - KDContextMenu (non LiteSync syncs)
         *  - FileExplorerExtension (LiteSync syncs): Context menu, Cloud provider, Thumbnail provider
         */
        void executeCommand(const CommString &commandLineStr, std::shared_ptr<AbstractCommChannel> channel);

        // Commands...
        // From FinderSyncExt & KDContextMenu
        /** Request for the list of menu items.
         * argument is a list of files for which the menu should be shown, separated by '\x1e'
         * Reply with  GET_MENU_ITEMS:BEGIN
         * followed by several MENU_ITEM:[Action]:[flag]:[Text]
         * If flag contains 'd', the menu should be disabled
         * and ends with GET_MENU_ITEMS:END
         */
        void commandGetMenuItems(const CommString &argument, std::shared_ptr<AbstractCommChannel> channel);
        // From FinderSyncExt, KDContextMenu & FileExplorerExtension (Context menu)
        void commandCopyPublicLink(const CommString &argument, std::shared_ptr<AbstractCommChannel>);
        void commandCopyPrivateLink(const CommString &argument, std::shared_ptr<AbstractCommChannel> channel);
        void commandOpenPrivateLink(const CommString &argument, std::shared_ptr<AbstractCommChannel> channel);
        // From FinderSyncExt, LiteSyncExt & FileExplorerExtension (Cloud provider)
        void commandMakeAvailableLocallyDirect(const CommString &argument, std::shared_ptr<AbstractCommChannel> channel);
#ifdef _WIN32
        // From FileExplorerExtension (Context menu)
        void commandGetAllMenuItems(const CommString &argument, std::shared_ptr<AbstractCommChannel> channel);
        // From FileExplorerExtension (Thumbnail provider)
        void commandGetThumbnail(const CommString &argument, std::shared_ptr<AbstractCommChannel> channel);
#endif
#ifdef __APPLE__
        // From FinderSyncExt
        void commandRetrieveFolderStatus(const CommString &argument, std::shared_ptr<AbstractCommChannel> channel);
        void commandRetrieveFileStatus(const CommString &argument, std::shared_ptr<AbstractCommChannel> channel);
        void commandMakeOnlineOnlyDirect(const CommString &argument, std::shared_ptr<AbstractCommChannel>);
        void commandCancelDehydrationDirect(const CommString &, std::shared_ptr<AbstractCommChannel>);
        void commandCancelHydrationDirect(const CommString &argument, std::shared_ptr<AbstractCommChannel>);
        // From LiteSyncExt
        void commandSetThumbnail(const CommString &argument, std::shared_ptr<AbstractCommChannel>);
#endif

        // Fetch the private link and call targetFun
        void fetchPrivateLinkUrlHelper(const SyncPath &localFile, const std::function<void(const std::string &url)> &targetFun);

        void sendSharingContextMenuOptions(const FileData &fileData, std::shared_ptr<AbstractCommChannel> channel);
        void addSharingContextMenuOptions(const FileData &fileData, CommString &response);

        void manageActionsOnSingleFile(std::shared_ptr<AbstractCommChannel> channel, const std::vector<CommString> &files,
                                       std::unordered_map<int, std::shared_ptr<SyncPal>>::const_iterator syncPalMapIt,
                                       std::unordered_map<int, std::shared_ptr<Vfs>>::const_iterator vfsMapIt, const Sync &sync);
        void buildAndSendRegisterPathMessage(std::shared_ptr<AbstractCommChannel> channel, const SyncPath &path);
        void buildAndSendMenuItemMessage(std::shared_ptr<AbstractCommChannel> channel, const CommString &type, bool enabled,
                                         const CommString &text);

        void processFileList(const std::vector<CommString> &inFileList, std::vector<SyncPath> &outFileList);
        bool syncFileStatus(const FileData &fileData, SyncFileStatus &status, VfsStatus &vfsStatus);
        ExitInfo setPinState(const FileData &fileData, PinState pinState);
        ExitInfo dehydratePlaceholder(const FileData &fileData);
        bool addDownloadJob(const FileData &fileData);
        bool cancelDownloadJobs(int syncDbId, const std::vector<CommString> &fileList);

        // Commands texts translated
        CommString vfsPinActionText();
        CommString vfsFreeSpaceActionText();
        CommString cancelDehydrationText();
        CommString cancelHydrationText();
        CommString resharingText(bool isDirectory);
        CommString copyPublicShareLinkText();
        CommString copyPrivateShareLinkText();
        CommString openInBrowserText();

        static bool openBrowser(const std::string &url);

        std::string statusString(SyncFileStatus status, const VfsStatus &vfsStatus) const;

        // Try to retrieve the Sync object with DB ID `syncDbId`.
        // Returns `false`, add errors and log messages on failure.
        // Returns `true` and set `sync` with the result otherwise.
        bool tryToRetrieveSync(const int syncDbId, Sync &sync) const;

        // Retrieve map iterators.
        // Returns the end() iterator on failure but also add an error and log a message in this case.
        std::unordered_map<int, std::shared_ptr<Vfs>>::const_iterator retrieveVfsMapIt(const int syncDbId) const;
        std::unordered_map<int, std::shared_ptr<SyncPal>>::const_iterator retrieveSyncPalMapIt(const int syncDbId) const;

        static void copyUrlToClipboard(const std::string &link);
        static void openPrivateLink(const std::string &link);
        static CommString buildMessage(const std::string &verb, const SyncPath &path, const std::string &status = "");
};

} // namespace KDC
