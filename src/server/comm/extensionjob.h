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
#include "libsyncengine/jobs/abstractjob.h"
#include "libsyncengine/syncpal/syncpal.h"
#include "vfs/vfs.h"

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

class CommManager;

class ExtensionJob : public AbstractJob {
    public:
        ExtensionJob(std::shared_ptr<CommManager> commManager, const CommString &commandLineStr,
                     const std::list<std::shared_ptr<AbstractCommChannel>> &channels);
        ~ExtensionJob() {}

        void runJob() override;

    private:
        std::shared_ptr<CommManager> _commManager;
        CommString _commandLineStr;
        std::list<std::shared_ptr<AbstractCommChannel>> _channels;

        static bool _dehydrationCanceled;
        static unsigned _nbOfOngoingDehydration;
        static std::mutex _dehydrationMutex;

        // Commands map
        std::map<std::string, std::function<void(const CommString &, std::shared_ptr<AbstractCommChannel>)>> _commands;

        // Commands...
        // To FinderSyncExt & FileExplorerExtension
        /**
         * @brief commandRegisterFolder
         * @param argument is a list of values separated by messageArgSeparator
         *  - value 1: path
         * @param channel
         */
        void commandRegisterFolder(const CommString &argument, std::shared_ptr<AbstractCommChannel> channel);
        /**
         * @brief commandUnregisterFolder
         * @param argument is a list of values separated by messageArgSeparator
         *  - value 1: path
         * @param channel
         */
        void commandUnregisterFolder(const CommString &argument, std::shared_ptr<AbstractCommChannel> channel);
        /**
         * @brief commandForceStatus
         * @param argument is a list of values separated by messageArgSeparator
         *  - value 1: isSyncing (bool)
         *  - value 2: progress ({0, 5, 10, ..., 90, 95, 100})
         *  - value 3: isHydrated (bool)
         *  - value 4: path
         * @param channel
         */
        void commandForceStatus(const CommString &argument, std::shared_ptr<AbstractCommChannel> channel);
        // From FinderSyncExt & KDContextMenu
        /** Request for values corresponding to keys.
         * argument is a list of keys for which the values are needed (empty for all keys)
         * Reply with  GET_STRINGS:BEGIN
         * followed by several STRING:[Key]:[Value]
         * and ends with GET_STRINGS:END
         */
        void commandGetStrings(const CommString &argument, std::shared_ptr<AbstractCommChannel> channel);
        /** Request for the list of menu items.
         * argument is a list of files for which the menu should be shown, separated by messageArgSeparator
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
        // From FinderSyncExt & KDOverlays
        void commandRetrieveFileStatus(const CommString &argument, std::shared_ptr<AbstractCommChannel> channel);
#if defined(KD_WINDOWS)
        // From FileExplorerExtension (Context menu)
        void commandGetAllMenuItems(const CommString &argument, std::shared_ptr<AbstractCommChannel> channel);
        // From FileExplorerExtension (Thumbnail provider)
        void commandGetThumbnail(const CommString &argument, std::shared_ptr<AbstractCommChannel> channel);
#endif
#if defined(KD_MACOS)
        // From FinderSyncExt
        void commandRetrieveFolderStatus(const CommString &argument, std::shared_ptr<AbstractCommChannel> channel);
        void commandMakeOnlineOnlyDirect(const CommString &argument, std::shared_ptr<AbstractCommChannel>);
        void commandCancelDehydrationDirect(const CommString &, std::shared_ptr<AbstractCommChannel>);
        void commandCancelHydrationDirect(const CommString &argument, std::shared_ptr<AbstractCommChannel>);
        // From LiteSyncExt
        void commandSetThumbnail(const CommString &argument, std::shared_ptr<AbstractCommChannel>);
#endif

        /** Execute commands requested by the extensions
         * macOS:
         *  - FinderSyncExt
         *  - LiteSyncExt
         * Windows:
         *  - KDContextMenu (non LiteSync syncs)
         *  - FileExplorerExtension (LiteSync syncs): Context menu, Cloud provider, Thumbnail provider
         */
        void executeCommand(const CommString &commandLineStr, std::shared_ptr<AbstractCommChannel> channel);

        void manageActionsOnSingleFile(std::shared_ptr<AbstractCommChannel> channel, const std::vector<CommString> &files,
                                       SyncPalMap::const_iterator syncPalMapIt, VfsMap::const_iterator vfsMapIt,
                                       const Sync &sync);

        void fetchPrivateLinkUrlHelper(const SyncPath &localFile, const std::function<void(const std::string &url)> &targetFun);

        void sendSharingContextMenuOptions(const FileData &fileData, std::shared_ptr<AbstractCommChannel> channel);
        void addSharingContextMenuOptions(const FileData &fileData, CommString &response);

        void buildAndSendMenuItemMessage(std::shared_ptr<AbstractCommChannel> channel, const CommString &type, bool enabled,
                                         const CommString &text);

        void processFileList(const std::vector<CommString> &inFileList, std::vector<SyncPath> &outFileList);
        bool syncFileStatus(const FileData &fileData, SyncFileStatus &status, VfsStatus &vfsStatus);
        ExitInfo setPinState(const FileData &fileData, PinState pinState);
        ExitInfo dehydratePlaceholder(const FileData &fileData);
        bool addDownloadJob(const FileData &fileData, const SyncPath &parentFolderPath);
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

        // Retrieve map iterators.
        // Returns the end() iterator on failure but also add an error and log a message in this case.
        VfsMap::const_iterator retrieveVfsMapIt(const int syncDbId) const;
        SyncPalMap::const_iterator retrieveSyncPalMapIt(const int syncDbId) const;

        static void copyUrlToClipboard(const std::string &link);
        static void openPrivateLink(const std::string &link);
        static CommString buildMessage(const std::string &verb, const SyncPath &path, const std::string &status = "");

        void monitorFolderHydration(const FileData &fileData) const;
};

} // namespace KDC
