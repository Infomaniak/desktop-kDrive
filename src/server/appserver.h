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

#include "qtsingleapplication.h"
#include "commserver.h"
#include "navigationpanehelper.h"
#include "socketapi.h"
#include "config.h"
#include "libcommon/systray/systray.h"
#include "syncpal/syncpal.h"
#include "libparms/db/user.h"
#include "libcommon/info/userinfo.h"
#include "libcommon/info/accountinfo.h"
#include "libcommon/info/driveinfo.h"
#include "libcommon/info/syncinfo.h"
#include "libcommon/info/syncfileiteminfo.h"
#include "libcommonserver/vfs/vfs.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QPointer>
#include <QQueue>
#include <QTimer>

namespace KDC {
class UpdateManager;

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
                SyncProgress _progress;

                bool operator==(const SyncCache &other) const {
                    return _status == other._status && _step == other._step && _progress == other._progress;
                }
                bool operator!=(const SyncCache &other) const { return !(*this == other); }
        };

        struct Notification {
                int _syncDbId;
                QString _filename;
                QString _renameTarget;
                SyncFileInstruction _status;
        };

        explicit AppServer(int &argc, char **argv);
        ~AppServer();

        void init();
        virtual void cleanup();
        static void reset();

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
        void sendRestartClientMsg();

        void clearKeychainKeys();

        void showHint(std::string errorHint);

    private:
        QStringList _arguments;
        log4cplus::Logger _logger;
        static std::unordered_map<int, std::shared_ptr<SyncPal>> _syncPalMap;
        static std::unordered_map<int, std::shared_ptr<Vfs>> _vfsMap;
        static std::vector<Notification> _notifications;

        std::unique_ptr<NavigationPaneHelper> _navigationPaneHelper;
        QScopedPointer<SocketApi> _socketApi;
        bool _appRestartRequired{false};
        bool _helpAsked{false};
        bool _versionAsked{false};
        bool _clearSyncNodesAsked{false};
        bool _settingsAsked{false};
        bool _synthesisAsked{false};
        bool _clearKeychainKeysAsked{false};
        bool _vfsInstallationDone{false};
        bool _vfsActivationDone{false};
        bool _vfsConnectionDone{false};
        bool _crashRecovered{false};
        bool _appStartPTraceStopped{false};
        bool _clientManuallyRestarted{false};
        QElapsedTimer _startedAt;
        QTimer _loadSyncsProgressTimer;
        QTimer _sendFilesNotificationsTimer;
        QTimer _restartSyncsTimer;
        std::unordered_map<int, SyncCache> _syncCacheMap;
        std::unordered_map<int, NodeSet> _undecidedListCacheMap;

        static std::unique_ptr<UpdateManager> _updateManager;

#ifdef Q_OS_LINUX
        QAction *_actionSynthesis = nullptr;
        QAction *_actionPreferences = nullptr;
        QAction *_actionQuit = nullptr;
#endif

        QScopedPointer<Systray> _tray;
        void resetSystray(bool lockedAppVersion = false);

        virtual std::filesystem::path makeDbName();
        virtual std::shared_ptr<ParmsDb> initParmsDB(const std::filesystem::path &dbPath, const std::string &version);
        virtual bool startClient();

        void parseOptions(const QStringList &options);
        bool initLogging() noexcept;
        void logUsefulInformation() const;
        bool setupProxy() noexcept;
        void handleCrashRecovery(bool &shouldQuit); // Sets `shouldQuit` with true if the crash recovery is successful, false if
                                                    // the application should exit.
        bool serverCrashedRecently(int seconds = 60 /*Allow one server self restart per minute (default)*/);
        bool clientCrashedRecently(int second = 60 /*Allow one client self restart per minute (default)*/);
        void processInterruptedLogsUpload();

        ExitCode migrateConfiguration(bool &proxyNotSupported);
        ExitCode updateUserInfo(User &user);
        ExitCode updateAllUsersInfo();
        [[nodiscard]] ExitInfo initSyncPal(const Sync &sync, const NodeSet &blackList = {}, const NodeSet &undecidedList = {},
                                           const NodeSet &whiteList = {}, bool start = true,
                                           const std::chrono::seconds &startDelay = std::chrono::seconds(0),
                                           bool resumedByUser = false, bool firstInit = false);
        [[nodiscard]] ExitInfo initSyncPal(const Sync &sync, const QSet<QString> &blackList, const QSet<QString> &undecidedList,
                                           const QSet<QString> &whiteList, bool start = true,
                                           const std::chrono::seconds &startDelay = std::chrono::seconds(0),
                                           bool resumedByUser = false, bool firstInit = false);
        [[nodiscard]] ExitInfo stopSyncPal(int syncDbId, bool pausedByUser = false, bool quit = false, bool clear = false);

        [[nodiscard]] ExitInfo createAndStartVfs(const Sync &sync) noexcept;
        // Call createAndStartVfs. Issue warnings, errors and pause the synchronization `sync` if needed.
        [[nodiscard]] ExitInfo tryCreateAndStartVfs(const Sync &sync) noexcept;
        [[nodiscard]] ExitInfo stopVfs(int syncDbId, bool unregister);

        [[nodiscard]] ExitInfo setSupportsVirtualFiles(int syncDbId, bool value);

        void startSyncsAndRetryOnError();
        [[nodiscard]] ExitInfo startSyncs();
        [[nodiscard]] ExitInfo startSyncs(User &user);
        [[nodiscard]] ExitInfo processMigratedSyncOnceConnected(int userDbId, int driveId, Sync &sync, QSet<QString> &blackList,
                                                                QSet<QString> &undecidedList, bool &syncUpdated);
        ExitCode clearErrors(int syncDbId, bool autoResolved = false);
        // Check if the synchronization `sync` is registred in the sync database and
        // if the `sync` folder does not contain any other sync subfolder.
        [[nodiscard]] ExitInfo checkIfSyncIsValid(const Sync &sync);

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
        void sendSyncProgressInfo(int syncDbId, SyncStatus status, SyncStep step, const SyncProgress &progress);
        void sendSyncAdded(const SyncInfo &syncInfo);
        void sendSyncUpdated(const SyncInfo &syncInfo);
        void sendSyncRemoved(int syncDbId);
        void sendSyncDeletionFailed(int syncDbId);
        void sendGetFolderSizeCompleted(const QString &nodeId, qint64 size);
        void sendNewBigFolder(int syncDbId, const QString &path);
        static void sendErrorsCleared(int syncDbId);
        void sendQuit(); // Ask client to quit

        void uploadLog(bool includeArchivedLogs);
        void sendLogUploadStatusUpdated(LogUploadState status, int percent);

        void stopSyncTask(int syncDbId); // Long task which can block GUI: post-poned in the event loop by means of timer
        void stopAllSyncsTask(const std::vector<int> &syncDbIdList); // Idem.
        void deleteAccount(int accountDbId);
        void deleteDrive(int driveDbId);
        void deleteSync(int syncDbId);

        static void addError(const Error &error);
        static void sendErrorAdded(bool serverLevel, ExitCode exitCode, int syncDbId);
        static void addCompletedItem(int syncDbId, const SyncFileItem &item, bool notify);
        static void sendSignal(SignalNum sigNum, int syncDbId, const SigValueType &val);

        static ExitInfo getVfsPtr(int syncDbId, std::shared_ptr<Vfs> &vfs);

        static void syncFileStatus(int syncDbId, const KDC::SyncPath &path, KDC::SyncFileStatus &status);
        static void syncFileSyncing(int syncDbId, const KDC::SyncPath &path, bool &syncing);
        static void setSyncFileSyncing(int syncDbId, const KDC::SyncPath &path, bool syncing);
#if defined(KD_MACOS)
        static void exclusionAppList(QString &appList);
#endif
        static void sendSyncCompletedItem(int syncDbId, const SyncFileItemInfo &item);
        static void sendVfsConversionCompleted(int syncDbId);
        static ExitCode sendShowFileNotification(int syncDbId, const QString &filename, const QString &renameTarget,
                                                 SyncFileInstruction status, int count);
        static void sendShowNotification(const QString &title, const QString &message);

        void showSettings();
        void showSynthesis();

        void logExtendedLogActivationMessage(bool isExtendedLogEnabled) noexcept;

        void updateSentryUser() const;

        bool clientHasCrashed() const;
        void handleClientCrash(bool &quit);

#if defined(KD_MACOS)
        bool noMacVfsSync() const;
        bool areMacVfsAuthsOk() const;
#endif

        // For testing purpose
        void crash() const;

        friend class TestAppServer;

    private slots:
        void onLoadInfo();
        void onUpdateSyncsProgress();
        void onSendFilesNotifications();
        void onRestartSyncs();
        void onScheduleAppRestart();
        void onShowWindowsUpdateDialog();
        void onUpdateStateChanged(UpdateState state);
        void onCleanup();
        void onRequestReceived(int id, RequestNum num, const QByteArray &params);
        void onRestartClientReceived();
        void onMessageReceivedFromAnotherProcess(const QString &message, QObject *);
        void onSendNotifAsked(const QString &title, const QString &message);

    signals:
        void socketApiExecuteCommandDirect(const QString &commandLine);
};
} // namespace KDC
