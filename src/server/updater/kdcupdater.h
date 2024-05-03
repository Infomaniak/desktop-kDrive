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

#include <QObject>
#include <QUrl>
#include <QTemporaryFile>
#include <QTimer>

#include "updateinfo.h"
#include "updaterserver.h"

class QNetworkAccessManager;
class QNetworkReply;

namespace KDC {

/**
 * @brief Schedule update checks every couple of hours if the client runs.
 * @ingroup gui
 *
 * This class schedules regular update checks. It also checks the config
 * if update checks are wanted at all.
 *
 * To reflect that all platforms have their own update scheme, a little
 * complex class design was set up:
 *
 * For Windows and Linux, the updaters are inherited from KDCUpdater, while
 * the MacOSX SparkleUpdater directly uses the class Updater. On windows,
 * NSISUpdater starts the update if a new version of the client is available.
 * On MacOSX, the sparkle framework handles the installation of the new
 * version. On Linux, the update capabilities of the underlying linux distro
 * are relied on, and thus the PassiveUpdateNotifier just shows a notification
 * if there is a new version once at every start of the application.
 */

class UpdaterScheduler : public QObject {
        Q_OBJECT
    public:
        UpdaterScheduler(QObject *parent);

    signals:
        void updaterAnnouncement(const QString &title, const QString &msg);
        void requestRestart();

    private slots:
        void slotTimerFired();

    private:
        QTimer _updateCheckTimer; /** Timer for the regular update check. */
};

class KDCUpdater : public UpdaterServer {
        Q_OBJECT
    public:
        enum DownloadState {
            Unknown = 0,
            CheckingServer,
            UpToDate,
            LastVersionSkipped,
            Downloading,
            DownloadComplete,
            DownloadFailed,
            DownloadTimedOut,
            UpdateOnlyAvailableThroughSystem
        };
        explicit KDCUpdater(const QUrl &url);

        void setUpdateUrl(const QUrl &url);

        bool performUpdate();

        void checkForUpdate() override;
        QString statusString() const override;

        int downloadState() const;
        void setDownloadState(DownloadState state);

        QString updateVersion();

    signals:
        void downloadStateChanged();
        void newUpdateAvailable(const QString &header, const QString &message);
        void requestRestart();

    public slots:
        // FIXME Maybe this should be in the NSISUpdater which should have been called WindowsUpdater
        void slotStartInstaller();

    protected slots:
        void backgroundCheckForUpdate() Q_DECL_OVERRIDE;
        void slotOpenUpdateUrl();

    private slots:
        void slotVersionInfoArrived();
        void slotTimedOut();

    protected:
        virtual void versionInfoArrived(const UpdateInfo &info) = 0;
        bool updateSucceeded() const;
        QNetworkAccessManager *qnam() const { return _accessManager; }
        UpdateInfo updateInfo() const { return _updateInfo; }

    private:
        QUrl _updateUrl;
        int _state;
        QNetworkAccessManager *_accessManager;
        QTimer *_timeoutWatchdog; /** Timer to guard the timeout of an individual network request */
        UpdateInfo _updateInfo;
};

/**
 * @brief Windows Updater Using NSIS
 * @ingroup gui
 */
class NSISUpdater : public KDCUpdater {
        Q_OBJECT
    public:
        explicit NSISUpdater(const QUrl &url);
        bool handleStartup() Q_DECL_OVERRIDE;

        void wipeUpdateData();
        void getVersions(QString &targetVersion, QString &targetVersionString, QString &clientVersion);
        bool autoUpdateAttempted();

    public slots:
        void slotSetSeenVersion();

    private slots:
        void slotDownloadFinished();
        void slotWriteFile();

    private:
        void showNoUrlDialog(const UpdateInfo &info);
        void versionInfoArrived(const UpdateInfo &info) Q_DECL_OVERRIDE;
        QScopedPointer<QTemporaryFile> _file;
        QString _targetFile;
};

/**
 *  @brief Updater that only implements notification for use in settings
 *
 *  The implementation does not show popups
 *
 *  @ingroup gui
 */
class PassiveUpdateNotifier : public KDCUpdater {
        Q_OBJECT
    public:
        explicit PassiveUpdateNotifier(const QUrl &url);
        bool handleStartup() Q_DECL_OVERRIDE { return false; }

    private:
        void versionInfoArrived(const UpdateInfo &info) Q_DECL_OVERRIDE;
};
}  // namespace KDC
