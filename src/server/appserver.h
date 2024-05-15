/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#endif

#include "qtsingleapplication.h"
#include "libcommonserver/commserver.h"
#include "login/login.h"
#include "syncpal/syncpal.h"
#include "libcommonserver/vfs.h"
#include "navigationpanehelper.h"
#include "socketapi.h"
#include "libparms/db/user.h"
#include "libcommon/info/userinfo.h"
#include "libcommon/info/accountinfo.h"
#include "libcommon/info/driveinfo.h"
#include "libcommon/info/syncinfo.h"
#include "libcommon/info/syncfileiteminfo.h"
#include "config.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QPointer>
#include <QQueue>
#include <QThread>
#include <QTimer>

namespace KDC {

class Theme;
/**
 * @brief The AppServer class
 * @ingroup gui
 */


class AppServer : public SharedTools::QtSingleApplication {
        Q_OBJECT
    public:
        struct SyncCache {
                SyncStatus _status;
                SyncStep _step;
                int64_t _currentFile;
                int64_t _totalFiles;
                int64_t _completedSize;
                int64_t _totalSize;
                int64_t _estimatedRemainingTime;
        };

        struct Notification {
                int _syncDbId;
                QString _filename;
                QString _renameTarget;
                SyncFileInstruction _status;
        };

        explicit AppServer(int &argc, char **argv);
        ~AppServer();

        inline bool helpAsked() { return _helpAsked; }
        inline bool versionAsked() { return _versionAsked; }
        inline bool clearSyncNodesAsked() { return _clearSyncNodesAsked; }
        inline bool settingsAsked() { return _settingsAsked; }
        inline bool synthesisAsked() { return _synthesisAsked; }
        inline bool clearKeychainKeysAsked() { return _clearKeychainKeysAsked; }
        void showHelp();
        void showVersion();
        void clearSyncNodes();
        void sendShowSettingsMsg();
        void sendShowSynthesisMsg();
        void clearKeychainKeys();
        void showAlreadyRunning();

        void showHint(std::string errorHint);
        bool debugMode();
        bool startClient();

    public slots:
        void onCrash();
        void onCrashEnforce();
        void onCrashFatal();

    private:
        log4cplus::Logger _logger;
        static std::unordered_map<int, std::shared_ptr<SyncPal>> _syncPalMap;
        static std::unordered_map<int, std::shared_ptr<Vfs>> _vfsMap;
        static std::vector<Notification> _notifications;
        static std::chrono::time_point<std::chrono::steady_clock> _lastSyncPalStart;

        std::unique_ptr<NavigationPaneHelper> _navigationPaneHelper;
        QScopedPointer<SocketApi> _socketApi;
        bool _appRestartRequired = false;
        Theme *_theme;
        bool _helpAsked = false;
        bool _versionAsked = false;
        bool _clearSyncNodesAsked = false;
        bool _settingsAsked = false;
        bool _synthesisAsked = false;
        bool _clearKeychainKeysAsked = false;
        bool _vfsInstallationDone = false;
        bool _vfsActivationDone = false;
        bool _vfsConnectionDone = false;
        bool _crashRecovered = false;
        QElapsedTimer _startedAt;
        QTimer _loadSyncsProgressTimer;
        QTimer _sendFilesNotificationsTimer;
        QTimer _restartSyncsTimer;
        std::unordered_map<int, SyncCache> _syncCacheMap;
        std::unordered_map<int, std::unordered_set<NodeId>> _undecidedListCacheMap;

        // options from command line:
        bool _debugMode;

        void parseOptions(const QStringList &);
        void initLogging() noexcept(false);
        void setupProxy();
        bool serverCrashedRecently(int seconds = 60 /*Allow one server self restart per minute (default)*/);
        bool clientCrashedRecently(int second = 60 /*Allow one client self restart per minute (default)*/);

        ExitCode migrateConfiguration(bool &proxyNotSupported);
        ExitCode updateUserInfo(User &user);
        ExitCode updateAllUsersInfo();
        ExitCode initSyncPal(const Sync &sync, const std::unordered_set<NodeId> &blackList = std::unordered_set<NodeId>(),
                             const std::unordered_set<NodeId> &undecidedList = std::unordered_set<NodeId>(),
                             const std::unordered_set<NodeId> &whiteList = std::unordered_set<NodeId>(), bool start = true,
                             bool resumedByUser = false, bool firstInit = false);
        ExitCode initSyncPal(const Sync &sync, const QSet<QString> &blackList, const QSet<QString> &undecidedList,
                             const QSet<QString> &whiteList, bool start = true, bool resumedByUser = false,
                             bool firstInit = false);
        ExitCode stopSyncPal(int syncDbId, bool pausedByUser = false, bool quit = false, bool clear = false);

        ExitCode createAndStartVfs(const Sync &sync, ExitCause &exitCause);
        ExitCode stopVfs(int syncDbId, bool unregister);

        ExitCode setSupportsVirtualFiles(int syncDbId, bool value);

        ExitCode startSyncs(ExitCause &exitCause);
        ExitCode startSyncs(User &user, ExitCause &exitCause);
        ExitCode processMigratedSyncOnceConnected(int userDbId, int driveId, Sync &sync, QSet<QString> &blackList,
                                                  QSet<QString> &undecidedList, bool &syncUpdated);
        ExitCode clearErrors(int syncDbId, bool autoResolved = false);
        ExitCode checkIfSyncIsValid(const Sync &sync);

        void sendUserAdded(const UserInfo &userInfo);
        static void sendUserUpdated(const UserInfo &userInfo);
        void sendUserStatusChanged(int userDbId, bool connected, QString connexionError);
        void sendUserRemoved(int userDbId);
        void sendAccountAdded(const AccountInfo &accountInfo);
        void sendAccountUpdated(const AccountInfo &accountInfo);
        void sendAccountRemoved(int accountDbId);
        void sendDriveAdded(const DriveInfo &driveInfo);
        void sendDriveUpdated(const DriveInfo &driveInfo);
        void sendDriveQuotaUpdated(int driveDbId, qint64 total, qint64 used);
        void sendDriveRemoved(int driveDbId);
        void sendDriveDeletionFailed(int driveDbId);
        void sendSyncProgressInfo(int syncDbId, SyncStatus status, SyncStep step, int64_t currentFile, int64_t totalFiles,
                                  int64_t completedSize, int64_t totalSize, int64_t estimatedRemainingTime);
        void sendSyncAdded(const SyncInfo &syncInfo);
        void sendSyncUpdated(const SyncInfo &syncInfo);
        void sendSyncRemoved(int syncDbId);
        void sendSyncDeletionFailed(int syncDbId);
        void sendGetFolderSizeCompleted(const QString &nodeId, qint64 size);
        void sendNewBigFolder(int syncDbId, const QString &path);
        void sendErrorsCleared(int syncDbId);
        
        // See types.h -> AppStateKey for the possible values of status
        void cancelLogUpload();
        void uploadLog(bool includeArchivedLogs);
        void sendLogUploadStatusUpdated(char status, int percent);

        void startSyncPals();
        void stopSyncTask(int syncDbId);  // Long task which can block GUI: post-poned in the event loop by means of timer
        void stopAllSyncsTask(const std::vector<int> &syncDbIdList);  // Idem.
        void deleteAccount(int accountDbId);

        static void addError(const Error &error);
        static void sendErrorAdded(bool serverLevel, ExitCode exitCode, int syncDbId);
        static void addCompletedItem(int syncDbId, const SyncFileItem &item, bool notify);
        static void sendSignal(int sigId, int syncDbId, const SigValueType &val);

        static bool vfsIsExcluded(int syncDbId, const SyncPath &itemPath, bool &isExcluded);
        static bool vfsExclude(int syncDbId, const SyncPath &itemPath);
        static bool vfsPinState(int syncDbId, const SyncPath &absolutePath, PinState &pinState);
        static bool vfsSetPinState(int syncDbId, const SyncPath &itemPath, PinState pinState);
        static bool vfsStatus(int syncDbId, const SyncPath &itemPath, bool &isPlaceholder, bool &isHydrated, bool &isSyncing,
                              int &progress);
        static bool vfsCreatePlaceholder(int syncDbIdconst, const SyncPath &relativeLocalPath, const SyncFileItem &item);
        static bool vfsConvertToPlaceholder(int syncDbId, const SyncPath &path, const SyncFileItem &item, bool &needRestart);
        static bool vfsUpdateMetadata(int syncDbId, const SyncPath &path, const SyncTime &creationTime, const SyncTime &modtime,
                                      const int64_t size, const NodeId &id, std::string &error);
        static bool vfsUpdateFetchStatus(int syncDbId, const SyncPath &tmpPath, const SyncPath &path, int64_t received,
                                         bool &canceled, bool &finished);
        static bool vfsFileStatusChanged(int syncDbId, const SyncPath &path, SyncFileStatus status);
        static bool vfsForceStatus(int syncDbId, const SyncPath &path, bool isSyncing, int progress, bool isHydrated = false);
        static bool vfsClearFileAttributes(int syncDbId, const SyncPath &path);
        static bool vfsCancelHydrate(int syncDbId, const SyncPath &path);

        static void syncFileStatus(int syncDbId, const KDC::SyncPath &path, KDC::SyncFileStatus &status);
        static void syncFileSyncing(int syncDbId, const KDC::SyncPath &path, bool &syncing);
        static void setSyncFileSyncing(int syncDbId, const KDC::SyncPath &path, bool syncing);
#ifdef Q_OS_MAC
        static void exclusionAppList(QString &appList);
#endif
        static void sendSyncCompletedItem(int syncDbId, const SyncFileItemInfo &item);
        static void sendVfsConversionCompleted(int syncDbId);
        static ExitCode sendShowFileNotification(int syncDbId, const QString &filename, const QString &renameTarget,
                                                 SyncFileInstruction status, int count);
        static void sendShowNotification(const QString &title, const QString &message);

        void showSettings();
        void showSynthesis();

    private slots:
        void onLoadInfo();
        void onUpdateSyncsProgress();
        void onSendFilesNotifications();
        void onRestartSyncs();
        void onScheduleAppRestart();
        void onShowWindowsUpdateErrorDialog();
        void onCleanup();
        void onRequestReceived(int id, RequestNum num, const QByteArray &params);
        void onRestartClientReceived();
        void onMessageReceivedFromAnotherProcess(const QString &message, QObject *);
};


}  // namespace KDC
