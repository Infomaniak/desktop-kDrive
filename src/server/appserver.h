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

#include <QFileOpenEvent>
#include <QUrl>
#include <QUrlQuery>
#if defined(KD_WINDOWS)
#include "navigationpanehelper.h"
#endif
#include "config.h"
#include "version.h"
#include "requests/serverrequests.h"
#include "comm/oldcommserver.h"
#include "comm/commmanager.h"
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
#include <QProcess>
#include <QQueue>
#include <QTimer>

namespace KDC {
class UpdateManager;

class Theme;
/**
 * @brief The AppServer class
 * @ingroup gui
 */

class AuthorizationCodeEventFilter : public QObject {
        // On macOS, the authorization code is retrieved by catching the `QEvent::FileOpen` and extracting its URL.
        Q_OBJECT

    signals:
        void authorizationCodeReceived(const QString &code, const QString &state);

    private:
        bool eventFilter(QObject *obj, QEvent *event) override {
            if (event->type() == QEvent::FileOpen) {
                const auto *openEvent = static_cast<const QFileOpenEvent *>(event);
                const auto url = openEvent->url();

                if (url.scheme() == "kdrive" && url.host() == "auth-desktop") {
                    const QUrlQuery query(url);
                    const auto code = query.queryItemValue("code");
                    const auto state = query.queryItemValue("state");

                    if (!code.isEmpty() && !state.isEmpty()) {
                        // We cannot log errors here. We forward the authorization code to the client only if values are
                        // non-empty.
                        emit authorizationCodeReceived(code, state);
                    }
                    return true;
                }
            }
            return QObject::eventFilter(obj, event);
        }
};

class AppServer : public SharedTools::QtSingleApplication {
        Q_OBJECT

    public:
        SyncPalMap syncPalMap;
        std::recursive_mutex syncPalMapMutex;

        VfsMap vfsMap;
        std::recursive_mutex vfsMapMutex;

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
                SyncDbId _syncDbId;
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
        inline bool authorizationCodeReceived() { return !_authorizationCodeStr.isEmpty(); }
        inline bool clearKeychainKeysAsked() { return _clearKeychainKeysAsked; }

        void showHelp();
        void showVersion();
        void clearSyncNodes();
        void sendShowSettingsMsg();
        void sendShowSynthesisMsg();
        void sendRestartClientMsg();
        void sendAuthorizationCode();

        void clearKeychainKeys();

        void showHint(std::string errorHint);

        void stopAllSyncPals();
        void stopAllVfs();

        void stopAllSyncsTask(const std::vector<SyncDbId> &syncDbIdList);

        void addError(const Error &error) const;
        void updateSentryUser();
        void deleteDrive(DriveDbId driveDbId);
        void deleteSync(SyncDbId syncDbId);
        ExitCode clearErrors(SyncDbId syncDbId, bool autoResolved = false);
        // Check if the synchronization `sync` is registered in the sync database and
        // if the `sync` folder does not contain any other sync subfolder.
        [[nodiscard]] ExitInfo checkIfSyncIsValid(const Sync &sync);
        //! Create and try to start the VFS plugin
        /*!
          \param sync is the sync whose VFS plugin must be initialized.
          \param postponed is true is the VFS plugin has not been initialized.
          \return ExitCode::Ok if no unexpected error occurred.
        */
        [[nodiscard]] ExitInfo tryCreateAndStartVfs(const Sync &sync, bool &startPostponed) noexcept;
        [[nodiscard]] ExitInfo getVfs(SyncDbId syncDbId, std::shared_ptr<Vfs> &vfs);
        [[nodiscard]] ExitInfo initSyncPal(const Sync &sync, const NodeSet &blackList = {}, bool start = true,
                                           const std::chrono::seconds &startDelay = std::chrono::seconds(0),
                                           bool resumedByUser = false, bool firstInit = false);
        [[nodiscard]] ExitInfo stopSyncPal(SyncDbId syncDbId, SyncPal::PauseCaller caller = SyncPal::PauseCaller::Sync,
                                           SyncPal::DbBehaviorAfterStop behavior = SyncPal::DbBehaviorAfterStop::Keep);
        void clearSyncCacheMap() { _syncCacheMap.clear(); }
        void triggerSyncProgressUpdate() { clearSyncCacheMap(); }

        void loadUsersInfo() { onLoadInfo(); }

        [[nodiscard]] ExitInfo stopVfs(SyncDbId syncDbId, bool unregister);
        [[nodiscard]] ExitInfo startSyncs(User &user);
        void stopSyncTask(SyncDbId syncDbId);
        [[nodiscard]] ExitInfo setSupportsVirtualFilesAsync(SyncDbId syncDbId, bool value);
        [[nodiscard]] ExitInfo setSupportsVirtualFiles(SyncDbId syncDbId, bool value);
        void setDistributionChannel(VersionChannel versionChannel);
        VersionInfo getVersionInfo(VersionChannel versionChannel) const;
        UpdateState getUpdateState() const;
        void startInstaller();
        [[nodiscard]] ExitInfo getNodePath(SyncDbId syncDbId, const NodeId &nodeId, CommString &path);

        void logExtendedLogActivationMessage(bool isExtendedLogEnabled) noexcept;

        [[nodiscard]] ExitInfo updateParametersAndPropagateChanges(const ParametersInfo &);
        [[nodiscard]] ExitInfo sendAppStartTrace();

        // Ask the Finder/File explorer Extension to register the folder
        void registerSync(std::shared_ptr<SyncPal> syncPal);

        // Ask the Finder/File explorer Extension to unregister the folder
        void unregisterSync(std::shared_ptr<SyncPal> syncPal);

        void uploadLog(bool includeArchivedLogs);


#if defined(KD_MACOS) || defined(KD_WINDOWS)
        ExitCode getThumbnail(const DriveDbId driveDbId, const NodeId &nodeId, int width, std::string &thumbnail) {
            return ServerRequests::getThumbnail(driveDbId, nodeId, width, thumbnail);
        }
        ExitCode getPublicLinkUrl(const DriveDbId driveDbId, const NodeId &nodeId, std::string &linkUrl) {
            return ServerRequests::getPublicLinkUrl(driveDbId, nodeId, linkUrl);
        }
#endif

        static std::shared_ptr<CommManager> commManager() { return _commManager; }

        static bool useOldCommServer() {
#if defined(KD_WINDOWS) || defined(KD_MACOS)
            return true; // (KDRIVE_VERSION_MAJOR < 4);
#else
            return true;
#endif
        }

        static bool useCommManager(bool checkIfInitialized = true) {
#if defined(KD_WINDOWS) || defined(KD_MACOS)
            if (checkIfInitialized)
                return _commManager != nullptr;
            else
                return true;
#else
            return false;
#endif
        }

    protected:
        // ServerRequests methods are accessible through std::function pointers in order to be mocked in tests
        std::function<ExitInfo(User &user, bool &updated)> _loadUserInfo =
                static_cast<ExitInfo (*)(User &user, bool &updated)>(&ServerRequests::loadUserInfo);
        std::function<ExitInfo(Account &account, bool &updated)> _loadAccountInfo =
                static_cast<ExitInfo (*)(Account &account, bool &updated)>(&ServerRequests::loadAccountInfo);
        std::function<ExitInfo(Drive &drive, const AccountId previousAccountId, AccountId &newAccountId, bool &updated,
                               bool &quotaUpdated)>
                _loadDriveInfo =
                        static_cast<ExitInfo (*)(Drive &drive, const AccountId previousAccountId, AccountId &newAccountId,
                                                 bool &updated, bool &quotaUpdated)>(&ServerRequests::loadDriveInfo);

    private:
        AuthorizationCodeEventFilter _eventFilter;

        QStringList _arguments;
        log4cplus::Logger _logger;
        static std::vector<Notification> _notifications;
        static std::shared_ptr<CommManager> _commManager;
        bool _appRestartRequired{false};
        Theme *_theme{nullptr};
        bool _helpAsked{false};
        bool _versionAsked{false};
        bool _clearSyncNodesAsked{false};
        bool _settingsAsked{false};
        bool _synthesisAsked{false};
        QString _authorizationCodeStr;
        bool _clearKeychainKeysAsked{false};
        bool _vfsInstallationDone{false};
        bool _vfsActivationDone{false};
        bool _vfsConnectionDone{false};
        bool _crashRecovered{false};
        bool _noUpdate{false};
        bool _appStartPTraceStopped{false};
        bool _clientManuallyRestarted{false};
        QElapsedTimer _startedAt;
        QTimer _loadSyncsProgressTimer;
        QTimer _sendFilesNotificationsTimer;
        QTimer _restartSyncsTimer;
        std::unordered_map<SyncDbId, SyncCache> _syncCacheMap;
        std::unordered_map<int, NodeSet> _undecidedListCacheMap;
        QProcess *_clientProcess = nullptr;

        static std::unique_ptr<UpdateManager> _updateManager;

        virtual std::filesystem::path makeDbName();
        virtual std::shared_ptr<ParmsDb> initParmsDB(const std::filesystem::path &dbPath, const std::string &version);
        virtual bool startClient();

        void parseOptions(const QStringList &options);
        bool initLogging() noexcept;
        void logUsefulInformation();
        bool setupProxy() noexcept;
        void handleCrashRecovery(bool &shouldQuit); // Sets `shouldQuit` with true if the crash recovery is successful, false if
                                                    // the application should exit.
        bool serverCrashedRecently(int seconds = 60 /*Allow one server self restart per minute (default)*/);
        bool clientCrashedRecently(int second = 60 /*Allow one client self restart per minute (default)*/);
        void processInterruptedLogsUpload();

        ExitCode migrateConfiguration(bool &proxyNotSupported);
        ExitInfo updateAllUsersInfo();
        ExitInfo updateUserInfo(User &user);
        ExitInfo updateUser(User &user);
        ExitInfo createAccount(Account &newAccount);
        ExitInfo updateAccount(Account &account);
        ExitInfo updateDrive(const User &user, const Account &account, Drive &drive);
        ExitInfo handleDriveAccessDenied(const Drive &drive);
        ExitInfo manageDriveMovedToAnotherAccount(const User &user, const Account &oldAccount, const AccountId newAccountId,
                                                  Drive &drive, bool &driveUpdated);

        [[nodiscard]] ExitInfo initSyncPal(const Sync &sync, const QSet<QString> &blackList, bool start = true,
                                           const std::chrono::seconds &startDelay = std::chrono::seconds(0),
                                           bool resumedByUser = false, bool firstInit = false);

        [[nodiscard]] ExitInfo createAndStartVfs(const Sync &sync) noexcept;
        [[nodiscard]] ExitInfo setSupportsVirtualFiles(SyncDbId syncDbId, bool value, bool asyncResponse);

        void startSyncsAndRetryOnError();
        [[nodiscard]] ExitInfo startSyncs();
        [[nodiscard]] ExitInfo processMigratedSyncOnceConnected(UserDbId userDbId, DriveId driveId, Sync &sync,
                                                                QSet<QString> &blackList, bool &syncUpdated);

        virtual void sendUserAdded(const UserInfo &userInfo) const;
        virtual void sendUserUpdated(const UserInfo &userInfo) const;
        virtual void sendUserStatusChanged(UserDbId userDbId, bool connected, QString connexionError) const;
        virtual void sendUserRemoved(UserDbId userDbId) const;
        virtual void sendAccountAdded(const AccountInfo &accountInfo) const;
        virtual void sendAccountUpdated(const AccountInfo &accountInfo) const;
        virtual void sendAccountRemoved(AccountDbId accountDbId) const;
        virtual void sendDriveAdded(const DriveInfo &driveInfo) const;
        virtual void sendDriveUpdated(const DriveInfo &driveInfo) const;
        virtual void sendDriveQuotaUpdated(DriveDbId driveDbId, qint64 total, qint64 used) const;
        virtual void sendDriveRemoved(DriveDbId driveDbId) const;
        virtual void sendDriveDeletionFailed(DriveDbId driveDbId) const;
        virtual void sendSyncProgressInfo(SyncDbId syncDbId, SyncStatus status, SyncStep step,
                                          const SyncProgress &progress) const;
        virtual void sendSyncAdded(const SyncInfo &syncInfo) const;
        virtual void sendSyncUpdated(const SyncInfo &syncInfo) const;
        virtual void sendSyncRemoved(SyncDbId syncDbId) const;
        virtual void sendSyncDeletionFailed(SyncDbId syncDbId) const;
        virtual void sendGetFolderSizeCompleted(const QString &nodeId, qint64 size) const;
        virtual void sendErrorsCleared(SyncDbId syncDbId) const;
        virtual void sendQuit() const; // Ask client to quit
        virtual void sendLogUploadStatusUpdated(LogUploadState status, int percent) const;
        virtual void sendNodeFixConflictedFilesCompleted(SyncDbId syncDbId, qint64 nbErrors) const;

        void deleteAccount(AccountDbId accountDbId);
        void sendErrorAdded(const ErrorInfo &errorInfo) const;
        void sendErrorRemoved(int64_t dbId) const;
        void addCompletedItem(SyncDbId syncDbId, const SyncFileItem &item, bool notify);
        void sendGuiSignal(std::shared_ptr<AbstractGuiJob> signal) const;

        void syncFileStatus(SyncDbId syncDbId, const KDC::SyncPath &path, KDC::SyncFileStatus &status);
        void syncFileSyncing(SyncDbId syncDbId, const KDC::SyncPath &path, bool &syncing);
        void setSyncFileSyncing(SyncDbId syncDbId, const KDC::SyncPath &path, bool syncing);
#if defined(KD_MACOS)
        void exclusionAppList(QString &appList);
#endif
        void sendSyncCompletedItem(SyncDbId syncDbId, const SyncFileItemInfo &item) const;
        void sendVfsConversionCompleted(SyncDbId syncDbId) const;
        ExitCode sendShowFileNotification(SyncDbId syncDbId, const QString &filename, const QString &renameTarget,
                                          SyncFileInstruction status, int count) const;
        void sendShowNotification(const QString &title, const QString &message) const;

        void showSettings();
        void showSynthesis();

        bool clientHasCrashed() const;
        void handleClientCrash(bool &quit);

#if defined(KD_MACOS)
        bool noMacVfsSync();
        bool areMacVfsAuthsOk() const;
#endif

        std::string appUID() const;

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
        void onUpdateRequired();
        void onUpdateStateChanged(UpdateState state);
        void onCleanup();
        void onRequestReceived(int id, RequestNum num, const QByteArray &params);
        void onRestartClientReceived();
        void onMessageReceivedFromAnotherProcess(const QString &message, QObject *);
        void onSendNotifAsked(const QString &title, const QString &message);
        void onAuthorizationCodeReceived(const QString &code, const QString &state);
};
} // namespace KDC
