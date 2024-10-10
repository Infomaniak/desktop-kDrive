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
        UpdateState updateState() const override;

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
        friend class TestUpdater;
        QUrl _updateUrl;
        int _state = Unknown;
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
        void slotUnsetSeenVersion();

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
} // namespace KDC
