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

#include "windowsupdater.h"

#include "libsyncengine/requests/parameterscache.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/log/log.h"
#include "libcommon/utility/utility.h"
#include "utility/utility.h"
#include "theme/theme.h"

#include <QObject>
#include <QtCore>
#include <QtNetwork>
#include <QtGui>
#include <QtWidgets>

namespace KDC {

static const double minSupportedWindowsMajorVersion = 10;
static const double minSupportedWindowsMinorVersion = 0;
static const double minSupportedWindowsMicroVersion = 17763;

WindowsUpdater::WindowsUpdater(const QUrl &url) : KDCUpdater(url) {}

void WindowsUpdater::slotWriteFile() {
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (_file->isOpen()) {
        _file->write(reply->readAll());
    }
}

void WindowsUpdater::wipeUpdateData() {
    QString updateFile = QString::fromStdString(ParametersCache::instance()->parameters().updateFileAvailable());
    if (!updateFile.isEmpty()) QFile::remove(updateFile);

    ParametersCache::instance()->parameters().setUpdateFileAvailable(std::string());
    ParametersCache::instance()->parameters().setUpdateTargetVersion(std::string());
    ParametersCache::instance()->parameters().setUpdateTargetVersionString(std::string());
    ParametersCache::instance()->parameters().setAutoUpdateAttempted(false);
    ExitCode exitCode = ParametersCache::instance()->save();
    if (exitCode != ExitCodeOk) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParametersCache::saveParameters");
        return;
    }
}

void WindowsUpdater::getVersions(QString &targetVersion, QString &targetVersionString, QString &clientVersion) {
    targetVersion = QString::fromStdString(ParametersCache::instance()->parameters().updateTargetVersion());
    targetVersionString = QString::fromStdString(ParametersCache::instance()->parameters().updateTargetVersionString());
    clientVersion = UpdaterServer::clientVersion();
}

bool WindowsUpdater::autoUpdateAttempted() {
    return ParametersCache::instance()->parameters().autoUpdateAttempted();
}

void WindowsUpdater::slotDownloadFinished() {
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        setDownloadState(DownloadFailed);
        return;
    }

    QUrl url(reply->url());
    _file->close();

    // remove previously downloaded but not used installer
    QFile oldTargetFile(QString::fromStdString(ParametersCache::instance()->parameters().updateFileAvailable()));
    if (oldTargetFile.exists()) {
        oldTargetFile.remove();
    }

    QFile::copy(_file->fileName(), _targetFile);
    setDownloadState(DownloadComplete);
    LOGW_INFO(Log::instance()->getLogger(), L"Downloaded" << Utility::s2ws(url.toString().toStdString()).c_str() << L"to"
                                                          << Utility::s2ws(_targetFile.toStdString()).c_str());

    ParametersCache::instance()->parameters().setUpdateTargetVersion(updateInfo().version().toStdString());
    ParametersCache::instance()->parameters().setUpdateTargetVersionString(updateInfo().versionString().toStdString());
    ParametersCache::instance()->parameters().setUpdateFileAvailable(_targetFile.toStdString());
    ExitCode exitCode = ParametersCache::instance()->save();
    if (exitCode != ExitCodeOk) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParametersCache::saveParameters");
        return;
    }
}

void WindowsUpdater::versionInfoArrived(const UpdateInfo &info) {
    qint64 infoVersion = Helper::stringVersionToInt(info.version());
    std::string seenString = ParametersCache::instance()->parameters().seenVersion();
    qint64 seenVersion = Helper::stringVersionToInt(QString::fromStdString(seenString));
    qint64 currVersion = Helper::currentVersionToInt();
    LOGW_INFO(Log::instance()->getLogger(),
              L"Version info arrived -" << L" Your version:" << currVersion << L" Skipped version:" << seenVersion
                                        << Utility::s2ws(seenString).c_str() << L" Available version:" << infoVersion
                                        << Utility::s2ws(info.version().toStdString()).c_str() << L" Available version string:"
                                        << Utility::s2ws(info.versionString().toStdString()).c_str() << L" Web url:"
                                        << Utility::s2ws(info.web().toStdString()).c_str() << L" Download url:"
                                        << Utility::s2ws(info.downloadUrl().toStdString()).c_str());

    if (info.version().isEmpty()) {
        LOG_INFO(Log::instance()->getLogger(), "No version information available at the moment");
        setDownloadState(UpToDate);
    } else if (infoVersion <= currVersion) {
        LOG_INFO(Log::instance()->getLogger(), "Client is on latest version!");
        setDownloadState(UpToDate);
    } else if (infoVersion <= seenVersion) {
        LOGW_INFO(Log::instance()->getLogger(),
                  L"Version " << seenVersion << Utility::s2ws(seenString).c_str() << L" was skipped!");
        setDownloadState(LastVersionSkipped);
    } else {
        QString url = info.downloadUrl();
        if (url.isEmpty()) {
            showNoUrlDialog(info);
        } else {
            // App support dir
            SyncPath targetPath(CommonUtility::getAppSupportDir());
            _targetFile = SyncName2QStr((targetPath / QStr2SyncName(url.mid(url.lastIndexOf('/') + 1))).native());
            if (QFile(_targetFile).exists()) {
                setDownloadState(DownloadComplete);
            } else {
                QNetworkReply *reply = qnam()->get(QNetworkRequest(QUrl(url)));
                connect(reply, &QIODevice::readyRead, this, &WindowsUpdater::slotWriteFile);
                connect(reply, &QNetworkReply::finished, this, &WindowsUpdater::slotDownloadFinished);
                setDownloadState(Downloading);
                _file.reset(new QTemporaryFile);
                _file->setAutoRemove(true);
                _file->open();
            }
        }
    }
}

void WindowsUpdater::showNoUrlDialog(const UpdateInfo &info) {
    // if the version tag is set, there is a newer version.
    QDialog *msgBox = new QDialog;
    msgBox->setAttribute(Qt::WA_DeleteOnClose);
    msgBox->setWindowFlags(msgBox->windowFlags() & ~Qt::WindowContextHelpButtonHint);

    QIcon infoIcon = msgBox->style()->standardIcon(QStyle::SP_MessageBoxInformation);
    int iconSize = msgBox->style()->pixelMetric(QStyle::PM_MessageBoxIconSize);

    msgBox->setWindowIcon(infoIcon);

    QVBoxLayout *layout = new QVBoxLayout(msgBox);
    QHBoxLayout *hlayout = new QHBoxLayout;
    layout->addLayout(hlayout);

    msgBox->setWindowTitle(tr("New Version Available"));

    QLabel *ico = new QLabel;
    ico->setFixedSize(iconSize, iconSize);
    ico->setPixmap(infoIcon.pixmap(iconSize));
    QLabel *lbl = new QLabel;
    QString txt = tr("<p>A new version of the %1 Client is available.</p>"
                     "<p><b>%2</b> is available for download. The installed version is %3.</p>")
                      .arg(CommonUtility::escape(Theme::instance()->appNameGUI()), CommonUtility::escape(info.versionString()),
                           CommonUtility::escape(clientVersion()));

    lbl->setText(txt);
    lbl->setTextFormat(Qt::RichText);
    lbl->setWordWrap(true);

    hlayout->addWidget(ico);
    hlayout->addWidget(lbl);


    QDialogButtonBox *bb = new QDialogButtonBox;
    QPushButton *skip = bb->addButton(tr("Skip this version"), QDialogButtonBox::ResetRole);
    QPushButton *reject = bb->addButton(tr("Skip this time"), QDialogButtonBox::AcceptRole);
    QPushButton *getupdate = bb->addButton(tr("Get update"), QDialogButtonBox::AcceptRole);

    connect(skip, &QAbstractButton::clicked, msgBox, &QDialog::reject);
    connect(reject, &QAbstractButton::clicked, msgBox, &QDialog::reject);
    connect(getupdate, &QAbstractButton::clicked, msgBox, &QDialog::accept);

    connect(skip, &QAbstractButton::clicked, this, &WindowsUpdater::slotSetSeenVersion);
    connect(getupdate, &QAbstractButton::clicked, this, &WindowsUpdater::slotOpenUpdateUrl);

    layout->addWidget(bb);

    msgBox->open();
}

bool WindowsUpdater::handleStartup() {
    // Check that we support current Windows version
    bool versionSupported = true;
    int majorVersion = QOperatingSystemVersion::current().majorVersion();
    int minorVersion = QOperatingSystemVersion::current().minorVersion();
    int microVersion = QOperatingSystemVersion::current().microVersion();
    if (majorVersion < minSupportedWindowsMajorVersion) {
        versionSupported = false;
    } else if (majorVersion == minSupportedWindowsMajorVersion) {
        // Major version ok, check minor version
        if (minorVersion < minSupportedWindowsMinorVersion) {
            versionSupported = false;
        } else if (minorVersion == minSupportedWindowsMinorVersion) {
            // Minor version ok, check micro version
            if (microVersion < minSupportedWindowsMicroVersion) {
                versionSupported = false;
            }
        }
    }

    if (!versionSupported) {
        LOG_WARN(Log::instance()->getLogger(), "Windows version not supported anymore, update check aborted.");
        return false;
    }

    QString updateFileName = QString::fromStdString(ParametersCache::instance()->parameters().updateFileAvailable());
    // has the previous run downloaded an update?
    if (!updateFileName.isEmpty() && QFile(updateFileName).exists()) {
        LOG_INFO(Log::instance()->getLogger(), "An updater file is available");
        // did it try to execute the update?
        if (ParametersCache::instance()->parameters().autoUpdateAttempted()) {
            if (updateSucceeded()) {
                // success: clean up
                LOG_INFO(Log::instance()->getLogger(),
                         "The requested update attempt has succeeded" << Helper::currentVersionToInt());
                wipeUpdateData();
                return false;
            } else {
                // auto update failed. Ask user what to do
                LOG_INFO(Log::instance()->getLogger(),
                         "The requested update attempt has failed"
                             << ParametersCache::instance()->parameters().updateTargetVersion().c_str());
                return false;
            }
        } else {
            LOG_INFO(Log::instance()->getLogger(), "Triggering an update");
            return performUpdate();
        }
    }
    return false;
}

void WindowsUpdater::slotSetSeenVersion() {
    ParametersCache::instance()->parameters().setSeenVersion(updateInfo().version().toStdString());
    ExitCode exitCode = ParametersCache::instance()->save();
    if (exitCode != ExitCodeOk) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParametersCache::saveParameters");
        return;
    }
}

}  // namespace KDC