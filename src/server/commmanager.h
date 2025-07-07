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

#include <QList>
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
        static FileData get(const KDC::SyncPath &path);
        FileData parentFolder() const;

        // Absolute path of the file locally
        QString localPath;

        // Relative path of the file
        QString relativePath;

        int syncDbId{0};
        int driveDbId{0};

        bool isDirectory{false};
        bool isLink{false};
        KDC::VirtualFileMode virtualFileMode{KDC::VirtualFileMode::Off};
};

class CommManager {
    public:
        explicit CommManager(const std::unordered_map<int, std::shared_ptr<KDC::SyncPal>> &syncPalMap,
                             const std::unordered_map<int, std::shared_ptr<KDC::Vfs>> &vfsMap);
        virtual ~CommManager();

        inline void setAddErrorCallback(void (*addError)(const KDC::Error &)) { _addError = addError; }
        inline void setGetThumbnailCallback(KDC::ExitCode (*getThumbnail)(int, KDC::NodeId, int, std::string &)) {
            _getThumbnail = getThumbnail;
        }
        inline void setGetPublicLinkUrlCallback(KDC::ExitCode (*getPublicLinkUrl)(int, const QString &, QString &)) {
            _getPublicLinkUrl = getPublicLinkUrl;
        }

        void registerSync(int syncDbId);
        void unregisterSync(int syncDbId);
        void executeCommandDirect(const QString &commandLine);

        static bool syncForPath(const std::filesystem::path &path, KDC::Sync &sync);

    private:
        const std::unordered_map<int, std::shared_ptr<KDC::SyncPal>> &_syncPalMap;
        const std::unordered_map<int, std::shared_ptr<KDC::Vfs>> &_vfsMap;

        QSet<int> _registeredSyncs;
        QList<CommListener> _listeners;
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
        void (*_addError)(const KDC::Error &error);
        KDC::ExitCode (*_getThumbnail)(int driveDbId, KDC::NodeId nodeId, int width, std::string &thumbnail);
        KDC::ExitCode (*_getPublicLinkUrl)(int driveDbId, const QString &nodeId, QString &linkUrl);

        // Send message to all listeners
        void broadcastMessage(const QString &msg, bool doWait = false);

        // Execute commands requested by the FinderSync and LiteSync extensions
        void executeCommand(const QString &commandLine, const CommListener *listener);

        // Commands map
        std::map<std::string, std::function<void(const SyncName &, CommListener *)>> _commands;

        // Commands from FinderSyncExt & FileExplorerExtension (Contextual menu)
        void commandCopyPublicLink(const SyncName &argument, CommListener *listener);
        void commandCopyPrivateLink(const SyncName &argument, CommListener *listener);
        void commandOpenPrivateLink(const SyncName &argument, CommListener *listener);
        void commandCancelDehydrationDirect(const SyncName &, CommListener *);
        void commandCancelHydrationDirect(const SyncName &, CommListener *);
        // Commands from FinderSyncExt (Contextual menu) & FileExplorerExtension (Cloud provider)
        void commandMakeAvailableLocallyDirect(const SyncName &argument, CommListener *listener);
#ifdef _WIN32
        // Commands from FileExplorerExtension (Contextual menu)
        void commandGetAllMenuItems(const SyncName &argument, CommListener *listener);
        // Commands from FileExplorerExtension (Thumbnail provider)
        void commandGetThumbnail(const SyncName &argument, CommListener *listener);
#endif
#ifdef __APPLE__
        // Commands from FinderSyncExt
        void commandRetrieveFolderStatus(const SyncName &argument, CommListener *listener);
        void commandRetrieveFileStatus(const SyncName &argument, CommListener *listener);
        // Commands from FinderSyncExt (Contextual menu)
        /** Request for the list of menu items.
         * argument is a list of files for which the menu should be shown, separated by '\x1e'
         * Reply with  GET_MENU_ITEMS:BEGIN
         * followed by several MENU_ITEM:[Action]:[flag]:[Text]
         * If flag contains 'd', the menu should be disabled
         * and ends with GET_MENU_ITEMS:END
         */
        void commandGetMenuItems(const SyncName &argument, CommListener *listener);
        void commandMakeOnlineOnlyDirect(const SyncName &argument, CommListener *listener);
        // Commands from LiteSyncExt
        void commandSetThumbnail(const SyncName &argument, CommListener *);
#endif

        // Fetch the private link and call targetFun
        void fetchPrivateLinkUrlHelper(const SyncPath &localFile, const std::function<void(const QString &url)> &targetFun);

        // Sends the context menu options relating to sharing to listener
        void sendSharingContextMenuOptions(const FileData &fileData, const CommListener *listener);
        void addSharingContextMenuOptions(const FileData &fileData, QTextStream &response);

        void manageActionsOnSingleFile(CommListener *listener, const QStringList &files,
                                       std::unordered_map<int, std::shared_ptr<KDC::SyncPal>>::const_iterator syncPalMapIt,
                                       std::unordered_map<int, std::shared_ptr<KDC::Vfs>>::const_iterator vfsMapIt,
                                       const KDC::Sync &sync);
        QString buildRegisterPathMessage(const QString &path);
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
        static bool openBrowser(const QUrl &url);

        QString statusString(SyncFileStatus status, const VfsStatus &vfsStatus) const;

        // Try to retrieve the Sync object with DB ID `syncDbId`.
        // Returns `false`, add errors and log messages on failure.
        // Returns `true` and set `sync` with the result otherwise.
        bool tryToRetrieveSync(const int syncDbId, KDC::Sync &sync) const;

        // Retrieve map iterators.
        // Returns the end() iterator on failure but also add an error and log a message in this case.
        std::unordered_map<int, std::shared_ptr<KDC::Vfs>>::const_iterator retrieveVfsMapIt(const int syncDbId) const;
        std::unordered_map<int, std::shared_ptr<KDC::SyncPal>>::const_iterator retrieveSyncPalMapIt(const int syncDbId) const;

        static void copyUrlToClipboard(const QString &link);
        static void openPrivateLink(const QString &link);
};

} // namespace KDC
