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

#include "libcommonserver/vfs/vfs.h"
#include "socketlistener.h"
#include "libcommon/utility/types.h"
#include "libsyncengine/syncpal/syncpal.h"

#include <unordered_map>

#include <QList>
#include <QMutex>
#include <QWaitCondition>
#include <QTemporaryFile>
#include <QTimer>

#if defined(Q_OS_MAC)
#include "socketapisocket_mac.h"
#else
#include <QLocalServer>
typedef QLocalServer SocketApiServer;
#endif

class QUrl;

namespace KDC {

struct FileData {
        FileData() = default;

        static FileData get(const QString &path);
        static FileData get(const KDC::SyncPath &path);
        FileData parentFolder() const;

        // Absolute path of the file locally
        QString absoluteLocalPath;

        // Relative path of the file
        QString relativePath;

        int syncDbId{0};
        int driveDbId{0};

        bool isDirectory{false};
        bool isLink{false};
        KDC::VirtualFileMode virtualFileMode{KDC::VirtualFileMode::Off};
};

class SocketApi : public QObject {
        Q_OBJECT

    public:
        explicit SocketApi(const std::unordered_map<int, std::shared_ptr<KDC::SyncPal>> &syncPalMap,
                           const std::unordered_map<int, std::shared_ptr<KDC::Vfs>> &vfsMap, QObject *parent = 0);
        virtual ~SocketApi();

        inline void setAddErrorCallback(void (*addError)(const KDC::Error &)) { _addError = addError; }
        inline void setGetThumbnailCallback(KDC::ExitCode (*getThumbnail)(int, KDC::NodeId, int, std::string &)) {
            _getThumbnail = getThumbnail;
        }
        inline void setGetPublicLinkUrlCallback(KDC::ExitCode (*getPublicLinkUrl)(int, const QString &, QString &)) {
            _getPublicLinkUrl = getPublicLinkUrl;
        }

        void unregisterSync(int syncDbId);
        void registerSync(int syncDbId);

        static bool syncForPath(const std::filesystem::path &path, KDC::Sync &sync);
    public slots:
        void executeCommandDirect(const QString &commandLine);

    private slots:
        void slotNewConnection();
        void onLostConnection();
        void slotSocketDestroyed(QObject *obj);
        void slotReadSocket();

        static void copyUrlToClipboard(const QString &link);
        static void openPrivateLink(const QString &link);

    private:
        const std::unordered_map<int, std::shared_ptr<SyncPal>> &_syncPalMap;
        const std::unordered_map<int, std::shared_ptr<Vfs>> &_vfsMap;

        QSet<int> _registeredSyncs;
        QList<SocketListener> _listeners;
        SocketApiServer _localServer;

        bool _dehydrationCanceled = false;
        unsigned _nbOngoingDehydration = 0;
        QMutex _dehydrationMutex;

        // Callbacks
        void (*_addError)(const Error &error);
        ExitCode (*_getThumbnail)(int driveDbId, NodeId nodeId, int width, std::string &thumbnail);
        ExitCode (*_getPublicLinkUrl)(int driveDbId, const QString &nodeId, QString &linkUrl);

        void broadcastMessage(const QString &msg, bool doWait = false);
        void executeCommand(const QString &commandLine, const SocketListener *listener);

        Q_INVOKABLE void command_RETRIEVE_FOLDER_STATUS(const QString &argument, SocketListener *listener);
        Q_INVOKABLE void command_RETRIEVE_FILE_STATUS(const QString &argument, SocketListener *listener);

        Q_INVOKABLE void command_VERSION(const QString &argument, SocketListener *listener);

        // The context menu actions
        Q_INVOKABLE void command_COPY_PUBLIC_LINK(const QString &localFile, SocketListener *listener);
        Q_INVOKABLE void command_COPY_PRIVATE_LINK(const QString &localFile, SocketListener *listener);
        Q_INVOKABLE void command_OPEN_PRIVATE_LINK(const QString &localFile, SocketListener *listener);
        Q_INVOKABLE void command_MAKE_AVAILABLE_LOCALLY_DIRECT(const QString &filesArg);
        Q_INVOKABLE void command_MAKE_ONLINE_ONLY_DIRECT(const QString &filesArg, SocketListener *listener);
        Q_INVOKABLE void command_CANCEL_DEHYDRATION_DIRECT(const QString &);
        Q_INVOKABLE void command_CANCEL_HYDRATION_DIRECT(const QString &);

        // Fetch the private link and call targetFun
        void fetchPrivateLinkUrlHelper(const QString &localFile, const std::function<void(const QString &url)> &targetFun);

        /** Sends translated/branded strings that may be useful to the integration */
        Q_INVOKABLE void command_GET_STRINGS(const QString &argument, SocketListener *listener);

        /** Sends the request URL to get a thumbnail */
#ifdef Q_OS_WIN
        Q_INVOKABLE void command_GET_THUMBNAIL(const QString &argument, SocketListener *listener);
#endif

#ifdef Q_OS_MAC
        Q_INVOKABLE void command_SET_THUMBNAIL(const QString &filePath);
#endif

        // Sends the context menu options relating to sharing to listener
        void sendSharingContextMenuOptions(const FileData &fileData, const SocketListener *listener);
        void addSharingContextMenuOptions(const FileData &fileData, QTextStream &response);

        /** Send the list of menu item. (added in version 1.1)
         * argument is a list of files for which the menu should be shown, separated by '\x1e'
         * Reply with  GET_MENU_ITEMS:BEGIN
         * followed by several MENU_ITEM:[Action]:[flag]:[Text]
         * If flag contains 'd', the menu should be disabled
         * and ends with GET_MENU_ITEMS:END
         */
        // Mac and Windows (sync without Lite Sync) menu
        Q_INVOKABLE void command_GET_MENU_ITEMS(const QString &argument, SocketListener *listener);
        void manageActionsOnSingleFile(SocketListener *listener, const QStringList &files,
                                       std::unordered_map<int, std::shared_ptr<KDC::SyncPal>>::const_iterator syncPalMapIt,
                                       std::unordered_map<int, std::shared_ptr<KDC::Vfs>>::const_iterator vfsMapIt,
                                       const KDC::Sync &sync);

#ifdef Q_OS_WIN
        // Windows (sync with Lite Sync) menu
        Q_INVOKABLE void command_GET_ALL_MENU_ITEMS(const QString &argument, SocketListener *listener);
#endif

        QString buildRegisterPathMessage(const QString &path);
        void processFileList(const QStringList &inFileList, std::list<SyncPath> &outFileList);
        bool syncFileStatus(const FileData &fileData, SyncFileStatus &status, VfsStatus &vfsStatus);
        ExitInfo setPinState(const FileData &fileData, PinState pinState);
        ExitInfo forceStatus(const FileData &fileData, const VfsStatus &status);
        ExitInfo dehydratePlaceholder(const FileData &fileData);
        bool addDownloadJob(const FileData &fileData, const SyncPath &parentFolderPath);
        void monitorFolderHydration(const FileData &fileData) const;
        bool cancelDownloadJobs(int syncDbId, const QStringList &fileList);

        QString vfsPinActionText();
        QString vfsFreeSpaceActionText();
        QString cancelDehydrationText();
        QString cancelHydrationText();
        static bool openBrowser(const QUrl &url);

        QString socketAPIString(SyncFileStatus status, const VfsStatus &vfsStatus) const;


        // Try to retrieve the Sync object with DB ID `syncDbId`.
        // Returns `false`, add errors and log messages on failure.
        // Returns `true` and set `sync` with the result otherwise.
        bool tryToRetrieveSync(const int syncDbId, KDC::Sync &sync) const;

        // Retrieve map iterators.
        // Returns the end() iterator on failure but also add an error and log a message in this case.
        std::unordered_map<int, std::shared_ptr<KDC::Vfs>>::const_iterator retrieveVfsMapIt(const int syncDbId) const;
        std::unordered_map<int, std::shared_ptr<KDC::SyncPal>>::const_iterator retrieveSyncPalMapIt(const int syncDbId) const;
};

} // namespace KDC
