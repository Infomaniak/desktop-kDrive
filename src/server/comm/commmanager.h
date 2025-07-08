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

#include "commlistener.h"
#include "libcommon/utility/types.h"
#include "libcommonserver/vfs/vfs.h"
#include "libsyncengine/syncpal/syncpal.h"

#include <unordered_map>

#include <QMutex>

#if defined(__APPLE__)
#include "commserver_mac.h"
#else
#include <QLocalServer>
typedef QLocalServer CommServer;
#endif

class QUrl;

namespace KDC {

struct FileData {
        FileData() = default;

        static FileData get(const QString &path);
        static FileData get(const SyncPath &path);
        FileData parentFolder() const;

        // Absolute path of the file locally
        QString localPath;

        // Relative path of the file
        QString relativePath;

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

        inline void setAddErrorCallback(void (*addError)(const Error &)) { _addError = addError; }
        inline void setGetThumbnailCallback(ExitCode (*getThumbnail)(int, NodeId, int, std::string &)) {
            _getThumbnail = getThumbnail;
        }
        inline void setGetPublicLinkUrlCallback(ExitCode (*getPublicLinkUrl)(int, const QString &, QString &)) {
            _getPublicLinkUrl = getPublicLinkUrl;
        }

        void registerSync(int syncDbId);
        void unregisterSync(int syncDbId);
        void executeCommandDirect(const CommString &commandLine);

        static bool syncForPath(const std::filesystem::path &path, Sync &sync);

    private:
        const std::unordered_map<int, std::shared_ptr<SyncPal>> &_syncPalMap;
        const std::unordered_map<int, std::shared_ptr<Vfs>> &_vfsMap;

        QSet<int> _registeredSyncs;
        std::list<std::shared_ptr<CommListener>> _listeners;
        CommServer _localServer;

        bool _dehydrationCanceled = false;
        unsigned _nbOngoingDehydration = 0;
        QMutex _dehydrationMutex;

        // Callbacks
        void onNewExtConnection();
        void onNewGuiConnection();
        void onLostExtConnection(AbstractIODevice *ioDevice);
        void onLostGuiConnection(AbstractIODevice *ioDevice);
        void onExtListenerDestroyed(AbstractIODevice *ioDevice);
        void onReadyRead(AbstractIODevice *ioDevice);
        void onQueryReceived(AbstractIODevice *ioDevice);

        // AppServer callbacks
        void (*_addError)(const Error &error);
        ExitCode (*_getThumbnail)(int driveDbId, NodeId nodeId, int width, std::string &thumbnail);
        ExitCode (*_getPublicLinkUrl)(int driveDbId, const QString &nodeId, QString &linkUrl);

        // Send message to all listeners
        void broadcastMessage(const CommString &msg, bool doWait = false);

        // Execute commands requested by the FinderSync and LiteSync extensions
        void executeCommand(const CommString &commandLineStr, std::shared_ptr<CommListener> listener);

        // Commands map
        std::map<std::string, std::function<void(const CommString &, std::shared_ptr<CommListener>)>> _commands;

        // Commands from FinderSyncExt & FileExplorerExtension (Contextual menu)
        void commandCopyPublicLink(const CommString &argument, std::shared_ptr<CommListener>);
        void commandCopyPrivateLink(const CommString &argument, std::shared_ptr<CommListener> listener);
        void commandOpenPrivateLink(const CommString &argument, std::shared_ptr<CommListener> listener);
        void commandCancelDehydrationDirect(const CommString &, std::shared_ptr<CommListener>);
        void commandCancelHydrationDirect(const CommString &, std::shared_ptr<CommListener>);
        // Commands from FinderSyncExt (Contextual menu) & FileExplorerExtension (Cloud provider)
        void commandMakeAvailableLocallyDirect(const CommString &argument, std::shared_ptr<CommListener> listener);
#ifdef _WIN32
        // Commands from FileExplorerExtension (Contextual menu)
        void commandGetAllMenuItems(const CommString &argument, std::shared_ptr<CommListener> listener);
        // Commands from FileExplorerExtension (Thumbnail provider)
        void commandGetThumbnail(const CommString &argument, std::shared_ptr<CommListener> listener);
#endif
#ifdef __APPLE__
        // Commands from FinderSyncExt
        void commandRetrieveFolderStatus(const CommString &argument, std::shared_ptr<CommListener> listener);
        void commandRetrieveFileStatus(const CommString &argument, std::shared_ptr<CommListener> listener);
        // Commands from FinderSyncExt (Contextual menu)
        /** Request for the list of menu items.
         * argument is a list of files for which the menu should be shown, separated by '\x1e'
         * Reply with  GET_MENU_ITEMS:BEGIN
         * followed by several MENU_ITEM:[Action]:[flag]:[Text]
         * If flag contains 'd', the menu should be disabled
         * and ends with GET_MENU_ITEMS:END
         */
        void commandGetMenuItems(const CommString &argument, std::shared_ptr<CommListener> listener);
        void commandMakeOnlineOnlyDirect(const CommString &argument, std::shared_ptr<CommListener> listener);
        // Commands from LiteSyncExt
        void commandSetThumbnail(const CommString &argument, std::shared_ptr<CommListener>);
#endif

        // Fetch the private link and call targetFun
        void fetchPrivateLinkUrlHelper(const SyncPath &localFile, const std::function<void(const QString &url)> &targetFun);

        // Sends the context menu options relating to sharing to listener
        void sendSharingContextMenuOptions(const FileData &fileData, std::shared_ptr<CommListener> listener);
        void addSharingContextMenuOptions(const FileData &fileData, QTextStream &response);

        void manageActionsOnSingleFile(std::shared_ptr<CommListener> listener, const std::vector<CommString> &files,
                                       std::unordered_map<int, std::shared_ptr<SyncPal>>::const_iterator syncPalMapIt,
                                       std::unordered_map<int, std::shared_ptr<Vfs>>::const_iterator vfsMapIt, const Sync &sync);
        void buildAndSendRegisterPathMessage(std::shared_ptr<CommListener> listener, const SyncPath &path);
        void buildAndSendMenuItemMessage(std::shared_ptr<CommListener> listener, const CommString &type, bool enabled,
                                         const CommString &text);
        void processFileList(const QStringList &inFileList, std::list<SyncPath> &outFileList);
        bool syncFileStatus(const FileData &fileData, SyncFileStatus &status, VfsStatus &vfsStatus);
        ExitInfo setPinState(const FileData &fileData, PinState pinState);
        ExitInfo dehydratePlaceholder(const FileData &fileData);
        bool addDownloadJob(const FileData &fileData);
        bool cancelDownloadJobs(int syncDbId, const QStringList &fileList);

        QString vfsPinActionText();
        QString vfsFreeSpaceActionText();
        QString cancelDehydrationText();
        QString cancelHydrationText();
        QString resharingText(bool isDirectory);
        QString copyPublicShareLinkText();
        QString copyPrivateShareLinkText();
        QString openInBrowserText();

        static bool openBrowser(const QUrl &url);

        std::string statusString(SyncFileStatus status, const VfsStatus &vfsStatus) const;

        // Try to retrieve the Sync object with DB ID `syncDbId`.
        // Returns `false`, add errors and log messages on failure.
        // Returns `true` and set `sync` with the result otherwise.
        bool tryToRetrieveSync(const int syncDbId, Sync &sync) const;

        // Retrieve map iterators.
        // Returns the end() iterator on failure but also add an error and log a message in this case.
        std::unordered_map<int, std::shared_ptr<Vfs>>::const_iterator retrieveVfsMapIt(const int syncDbId) const;
        std::unordered_map<int, std::shared_ptr<SyncPal>>::const_iterator retrieveSyncPalMapIt(const int syncDbId) const;

        static void copyUrlToClipboard(const QString &link);
        static void openPrivateLink(const QString &link);
        static CommString buildMessage(const std::string &verb, const SyncPath &path, const std::string &status = "");
};

} // namespace KDC
