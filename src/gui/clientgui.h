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

#include "systray.h"
#include "synthesispopover.h"
#include "parametersdialog.h"
#include "adddrivewizard.h"
#include "logindialog.h"
#include "info/userinfoclient.h"
#include "info/accountinfo.h"
#include "info/driveinfoclient.h"
#include "info/syncinfoclient.h"
#include "info/syncfileiteminfo.h"
#include "libcommon/info/accountinfo.h"

#include <QAction>
#include <QMenu>
#include <QObject>
#include <QPointer>
#include <QSize>
#include <QScreen>
#include <QTimer>

#include <map>

namespace KDC {

class AppClient;

class ClientGui : public QObject, public std::enable_shared_from_this<ClientGui> {
        Q_OBJECT

    public:
        explicit ClientGui(AppClient *parent = 0);

        void init();
        bool isConnected();
        void hideAndShowTray();
        void showSynthesisDialog();
        int driveErrorsCount(int driveDbId, bool unresolved) const;
        inline int generalErrorsCount() const { return _generalErrorsCounter; }
        inline int hasGeneralErrors() const { return _generalErrorsCounter > 0; }
        const QString folderPath(int syncDbId, const QString &filePath) const;
        inline const std::map<int, UserInfoClient> &userInfoMap() const noexcept { return _userInfoMap; }
        inline const std::map<int, AccountInfo> &accountInfoMap() const noexcept { return _accountInfoMap; }
        inline const std::map<int, DriveInfoClient> &driveInfoMap() const noexcept { return _driveInfoMap; }
        inline std::map<int, DriveInfoClient> &driveInfoMap() noexcept { return _driveInfoMap; }
        inline const std::map<int, SyncInfoClient> &syncInfoMap() const noexcept { return _syncInfoMap; }
        inline std::map<int, SyncInfoClient> &syncInfoMap() noexcept { return _syncInfoMap; }
        bool setCurrentUserDbId(int userDbId);
        inline int currentUserDbId() const { return _currentUserDbId; }
        bool setCurrentAccountDbId(int accountDbId);
        inline int currentAccountDbId() const { return _currentAccountDbId; }
        bool setCurrentDriveDbId(int driveDbId);
        inline int currentDriveDbId() const { return _currentDriveDbId; }
        void loadSyncInfoMap(int driveDbId, std::map<int, SyncInfoClient> &syncInfoMap);
        void activateLoadInfo(bool value = true);
        void computeOverallSyncStatus();
        bool loadInfoMaps();
        void openLoginDialog(int userDbId, bool invalidTokenError);
        void newDriveWizard(bool addDriveAccept = false);
        void getWebviewDriveLink(int userDbId, QString &driveLink);
        void errorInfoList(int driveDbId, QList<ErrorInfo> &errorInfoList);
        void resolveConflictErrors(int driveDbId, bool keepLocalVersion);
        void resolveUnsupportedCharErrors(int driveDbId);
        void closeAllExcept(const QWidget *exceptWidget);
        bool isUserUsed(int userDbId) const;

        static void restoreGeometry(QWidget *w);

    signals:
        void userListRefreshed();
        void accountListRefreshed();
        void driveListRefreshed();
        void syncListRefreshed();
        void driveQuotaUpdated(int driveDbId);
        void updateProgress(int syncDbId);
        void syncFinished(int syncDbId);
        void itemCompleted(int syncDbId, const SyncFileItemInfo &itemInfo);
        void vfsConversionCompleted(int syncDbId);
        void newBigFolder(int syncDbId, const QString &path);
        void errorAdded(int syncDbId);
        void appVersionLocked(bool currentVersionLocked);
        void errorsCleared(int syncDbId);
        void refreshStatusNeeded();
        void folderSizeCompleted(QString nodeId, qint64 size);
        void driveBeingRemoved();
        void logUploadStatusUpdated(LogUploadState status, int progress);
        void updateStateChanged(UpdateState state);

    public slots:
        // void onManageRightAndSharingItem(int syncDbId, const QString &filePath);
        void onCopyLinkItem(int syncDbId, const QString &nodeId);
        void onOpenWebviewItem(int userDbId, const QString &nodeId);
        void onShutdown();
        void onShowParametersDialog(int syncDbId = 0, bool errorPage = false);
        void onErrorAdded(bool serverLevel, ExitCode exitCode, int syncDbId);
        void onErrorsCleared(int syncDbId);
        void onFixConflictingFilesCompleted(int syncDbId, uint64_t nbErrors);
        void onNewDriveWizard();
        void onShowWindowsUpdateDialog(const KDC::VersionInfo &versionInfo) const;
        void onAppVersionLocked(bool currentVersionLocked);

    private:
        QScopedPointer<Systray> _tray;
        QScopedPointer<SynthesisPopover> _synthesisPopover;
        QScopedPointer<ParametersDialog> _parametersDialog;
        std::unique_ptr<AddDriveWizard> _addDriveWizard;
        std::unique_ptr<LoginDialog> _loginDialog;
        bool _workaroundShowAndHideTray{false};
        bool _workaroundNoAboutToShowUpdate{false};
        bool _workaroundFakeDoubleClick{false};
        bool _workaroundManualVisibility{false};
        QTimer _delayedTrayUpdateTimer;
        QDateTime _notificationEnableDate;
        AppClient *_app{nullptr};
        std::map<int, UserInfoClient> _userInfoMap;
        std::map<int, AccountInfo> _accountInfoMap;
        std::map<int, DriveInfoClient> _driveInfoMap;
        std::map<int, SyncInfoClient> _syncInfoMap;
        int _generalErrorsCounter{0};
        int _currentUserDbId{0};
        int _currentAccountDbId{0};
        int _currentDriveDbId{0};
        QSet<int> _driveWithNewErrorSet;
        QTimer _refreshErrorListTimer;
        std::map<int, QList<ErrorInfo>> _errorInfoMap;

#ifdef Q_OS_LINUX
        QAction *_actionSynthesis = nullptr;
        QAction *_actionPreferences = nullptr;
        QAction *_actionQuit = nullptr;
#endif
        /* On some Linux distributions, the tray icon cannot open the synthesis dialog,
         * it is necessary to use a menu to open the synthesis dialog.
         */
        bool osRequireMenuTray() const;
        static void raiseDialog(QWidget *raiseWidget);
        void setupSynthesisPopover();
        void setupParametersDialog();
        void updateSystrayNeeded();
        void resetSystray(bool lockedAppVersion = false);
        static QString shortGuiLocalPath(const QString &path);
        void computeTrayOverallStatus(SyncStatus &status, bool &unresolvedConflicts);
        QString trayTooltipStatusString(SyncStatus status, bool unresolvedConflicts, bool paused);
        void executeSyncAction(ActionType type, int syncDbId);
        void refreshErrorList(int driveDbId);
        ExitCode loadError(int driveDbId, int syncDbId, ErrorLevel level);
        ExitCode handleErrors(int driveDbId, int syncDbId, ErrorLevel level);

    private slots:
        void onShowTrayMessage(const QString &title, const QString &msg);
        void onUpdateSystray();
        void onShowOptionalTrayMessage(const QString &title, const QString &msg);
        void onActionSynthesisTriggered(bool checked = false);
        void onActionPreferencesTriggered(bool checked = false);
        void onTrayClicked(QSystemTrayIcon::ActivationReason reason);
        // void onRemoveDestroyedShareDialogs();
        void onDisableNotifications(NotificationsDisabled type, QDateTime value);
        void onApplyStyle();
        void onSetStyle(bool darkTheme);
        void onAddDriveAccepted();
        void onAddDriveRejected();
        void onAddDriveFinished();
        void onScreenUpdated(QScreen *screen);
        void onRefreshErrorList();

        // User slots
        void onUserAdded(const UserInfo &userInfo);
        void onUserUpdated(const UserInfo &userInfo);
        void onUserStatusChanged(int userDbId, bool connected, QString connexionError);
        void onRemoveUser(int userDbId);
        void onUserRemoved(int userDbId);
        // Account slots
        void onAccountAdded(const AccountInfo &accountInfo);
        void onAccountUpdated(const AccountInfo &accountInfo);
        void onAccountRemoved(int userDbId);
        // Drive slots
        void onDriveAdded(const DriveInfo &driveInfo);
        void onDriveUpdated(const DriveInfo &driveInfo);
        void onDriveQuotaUpdated(int driveDbId, qint64 total, qint64 used);
        void onRemoveDrive(int driveDbId);
        void onDriveRemoved(int driveDbId);
        void onDriveDeletionFailed(int driveDbId);
        // Sync slots
        void onSyncAdded(const SyncInfo &syncInfo);
        void onSyncUpdated(const SyncInfo &syncInfo);
        void onRemoveSync(int syncDbId);
        void onSyncRemoved(int syncDbId);
        void onSyncDeletionFailed(int syncDbId);
        void onProgressInfo(int syncDbId, SyncStatus status, SyncStep step, int64_t currentFile, int64_t totalFiles,
                            int64_t completedSize, int64_t totalSize, int64_t estimatedRemainingTime);
        void onExecuteSyncAction(ActionType type, ActionTarget target, int dbId);
        void onRefreshStatusNeeded();

        void retranslateUi();
};

} // namespace KDC
