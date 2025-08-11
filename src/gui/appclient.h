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
#include "clientgui.h"
#include "libcommongui/commclient.h"
#include "config.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QPointer>
#include <QQueue>
#include <QTimer>
#include <QLoggingCategory>

namespace KDC {

Q_DECLARE_LOGGING_CATEGORY(lcAppClient)

class Theme;

/**
 * @brief The AppClient class
 * @ingroup gui
 */
class AppClient : public SharedTools::QtSingleApplication {
        Q_OBJECT

    public:
        explicit AppClient(int &argc, char **argv);
        ~AppClient();

        bool debugCrash() const;

        void showParametersDialog();
        void showSynthesisDialog(const QRect &geometry);
        void updateSystrayIcon();
        void askUserToLoginAgain(int userDbId, QString userEmail, bool invalidTokenError);

    signals:
        // User signals
        void userAdded(const UserInfo &userInfo);
        void userUpdated(const UserInfo &userInfo);
        void userStatusChanged(int userDbId, bool connected, QString connexionError);
        void userRemoved(int userDbId);
        // Account signals
        void accountAdded(const AccountInfo &accountInfo);
        void accountUpdated(const AccountInfo &accountInfo);
        void accountRemoved(int accountDbId);
        // Drive signals
        void driveAdded(const DriveInfo &driveInfo);
        void driveUpdated(const DriveInfo &driveInfo);
        void driveQuotaUpdated(int driveDbId, qint64 total, qint64 used);
        void driveRemoved(int driveDbId);
        void driveDeletionFailed(int driveDbId);
        // Sync signals
        void syncAdded(const SyncInfo &syncInfo);
        void syncUpdated(const SyncInfo &syncInfo);
        void syncRemoved(int syncDbId);
        void syncProgressInfo(int syncDbId, SyncStatus status, SyncStep step, int64_t currentFile, int64_t totalFiles,
                              int64_t completedSize, int64_t totalSize, int64_t estimatedRemainingTime);
        void itemCompleted(int syncDbId, const SyncFileItemInfo &itemInfo);
        void vfsConversionCompleted(int syncDbId);
        void syncDeletionFailed(int syncDbId);
        // Node signals
        void folderSizeCompleted(QString nodeId, qint64 size);
        void fixConflictingFilesCompleted(int syncDbId, uint64_t nbErrors);
        // Utility
        void newBigFolder(int syncDbId, const QString &path);
        void showNotification(const QString &title, const QString &message);
        void errorAdded(bool serverLevel, ExitCode exitCode, int syncDbId);
        void errorsCleared(int syncDbId);
        void logUploadStatusUpdated(LogUploadState status, int percent);
        // Updater
        void updateStateChanged(UpdateState state);
        void showWindowsUpdateDialog(const VersionInfo &versionInfo);

    public slots:
        void onWizardDone(int);
        void onCrash();
        void onCrashServer();
        void onCrashEnforce();
        void onCrashFatal();

        /// Attempt to show() the tray icon again. Used if no systray was available initially.
        void onTryTrayAgain();

        void onQuit();

        void onServerDisconnected();

    private:
        bool parseOptions(const QStringList &);
        void setupLogging();

        bool serverHasCrashed();
        void startServerAndDie();
        bool connectToServer();
        void updateSentryUser() const;

        std::shared_ptr<ClientGui> _gui = nullptr;

        Theme *_theme = Theme::instance();

        QElapsedTimer _startedAt;
        quint16 _commPort = 0;

        // options from command line:
        QString _logFile;
        QString _logDir;
        std::chrono::hours _logExpire = std::chrono::hours(0);
        bool _logFlush = false;
        bool _logDebug = false;
        bool _debugCrash = false;
        bool _quitInProcess = false;

    private slots:
        void onUseMonoIconsChanged(bool);
        void onCleanup();
        void onSignalReceived(int id, SignalNum SignalNum, const QByteArray &params);
        void onLogTooBig();
};

} // namespace KDC
