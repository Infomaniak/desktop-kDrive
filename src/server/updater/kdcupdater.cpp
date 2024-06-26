#include "kdcupdater.h"
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

#include "config.h"
#include "kdcupdater.h"
#include "common/utility.h"
#include "libcommonserver/commserver.h"
#include "libcommon/theme/theme.h"
#include "libcommon/utility/types.h"
#include "libcommon/utility/utility.h"
#include "libparms/db/parmsdb.h"
#include "libparms/db/parameters.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/utility/utility.h"
#include "libsyncengine/requests/parameterscache.h"

#include <QObject>
#include <QtCore>
#include <QtNetwork>
#include <QtGui>
#include <QtWidgets>

#include <log4cplus/loggingmacros.h>

namespace KDC {

static const double minSupportedWindowsMajorVersion = 10;
static const double minSupportedWindowsMinorVersion = 0;
static const double minSupportedWindowsMicroVersion = 17763;

UpdaterScheduler::UpdaterScheduler(QObject *parent) : QObject(parent) {
    connect(&_updateCheckTimer, &QTimer::timeout, this, &UpdaterScheduler::slotTimerFired);

    // Note: the sparkle-updater is not an KDCUpdater
    if (KDCUpdater *updater = qobject_cast<KDCUpdater *>(UpdaterServer::instance())) {
        connect(updater, &KDCUpdater::newUpdateAvailable, this, &UpdaterScheduler::updaterAnnouncement);
        connect(updater, &KDCUpdater::requestRestart, this, &UpdaterScheduler::requestRestart);
    }

    // at startup, do a check in any case.
    QTimer::singleShot(3000, this, &UpdaterScheduler::slotTimerFired);

    auto checkInterval = std::chrono::hours(1);
    _updateCheckTimer.start(std::chrono::milliseconds(checkInterval).count());
}

void UpdaterScheduler::slotTimerFired() {
    UpdaterServer *updater = UpdaterServer::instance();
    if (updater) {
        updater->backgroundCheckForUpdate();
    }
}


/* ----------------------------------------------------------------- */

KDCUpdater::KDCUpdater(const QUrl &url)
    : UpdaterServer(),
      _updateUrl(url),
      _state(Unknown),
      _accessManager(new QNetworkAccessManager(this)),
      _timeoutWatchdog(new QTimer(this)) {}

void KDCUpdater::setUpdateUrl(const QUrl &url) {
    _updateUrl = url;
}

bool KDCUpdater::performUpdate() {
    QString updateFile = QString::fromStdString(ParametersCache::instance()->parameters().updateFileAvailable());
    if (!updateFile.isEmpty() && QFile(updateFile).exists() &&
        !updateSucceeded() /* Someone might have run the updater manually between restarts */) {
        const QString name = Theme::instance()->appNameGUI();
        if (QMessageBox::information(0, tr("New %1 Update Ready").arg(name),
                                     tr("A new update for %1 is about to be installed. The updater may ask\n"
                                        "for additional privileges during the process.")
                                         .arg(name),
                                     QMessageBox::Ok)) {
            slotStartInstaller();
            return true;
        }
    }
    return false;
}

void KDCUpdater::backgroundCheckForUpdate() {
    int dlState = downloadState();

    // do the real update check depending on the internal state of updater.
    switch (dlState) {
        case Unknown:
        case UpToDate:
        case DownloadFailed:
        case DownloadTimedOut:
            LOG_INFO(Log::instance()->getLogger(), "Checking for available update.");
            checkForUpdate();
            break;
        case DownloadComplete:
            LOG_INFO(Log::instance()->getLogger(), "Update is downloaded, skip new check.");
            break;
        case UpdateOnlyAvailableThroughSystem:
            LOG_INFO(Log::instance()->getLogger(), "Update available, skip check.");
            break;
    }
}

QString KDCUpdater::statusString() const {
    QString updateVersion = _updateInfo.versionString();

    switch (downloadState()) {
        case Downloading:
            return tr("Downloading %1. Please wait...").arg(updateVersion);
        case DownloadComplete:
        case LastVersionSkipped:
            return tr("An update is available: %1").arg(updateVersion);
        case DownloadFailed:
            return tr("Could not download update.<br>Please download it from <a style=\"%1\" href=\"%2\">here</a>.")
                .arg(CommonUtility::linkStyle, _updateInfo.web());
        case DownloadTimedOut:
            return tr("Could not check for new updates.");
        case UpdateOnlyAvailableThroughSystem:
            return tr("An update is available: %1.<br>Please download it from <a style=\"%2\" href=\"%3\">here</a>.")
                .arg(updateVersion, CommonUtility::linkStyle, APPLICATION_DOWNLOAD_URL);
        case CheckingServer:
            return tr("Checking update server...");
        case Unknown:
            return tr("Update status is unknown: Did not check for new updates.");
        case UpToDate:
        // fall through
        default:
            return tr("%1 is up to date!").arg(APPLICATION_NAME);
    }
}

UpdateState KDCUpdater::updateState() const {
    switch (downloadState()) {
        case Downloading:
            return UpdateState::Downloading;
        case DownloadComplete:
            return UpdateState::Ready;
        case LastVersionSkipped:
            return UpdateState::Skipped;
        case UpdateOnlyAvailableThroughSystem:
            return UpdateState::ManualOnly;
        case CheckingServer:
            return UpdateState::Checking;
        case UpToDate:
            return UpdateState::None;
        default:
            return UpdateState::Error;
    }
}

int KDCUpdater::downloadState() const {
    return _state;
}

void KDCUpdater::setDownloadState(DownloadState state) {
    auto oldState = _state;
    _state = state;
    emit downloadStateChanged();

    // show the notification if the download is complete (on every check)
    // or once for system based updates.
    if (_state == KDCUpdater::DownloadComplete ||
        (oldState != KDCUpdater::UpdateOnlyAvailableThroughSystem && _state == KDCUpdater::UpdateOnlyAvailableThroughSystem)) {
        emit newUpdateAvailable(tr("Update Check"), statusString());
    }
}

QString KDCUpdater::updateVersion() {
    return _updateInfo.version();
}

void KDCUpdater::slotStartInstaller() {
    QString updateFile = QString::fromStdString(ParametersCache::instance()->parameters().updateFileAvailable());

    ParametersCache::instance()->parameters().setAutoUpdateAttempted(true);
    ExitCode exitCode = ParametersCache::instance()->save();
    if (exitCode != ExitCodeOk) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParametersCache::saveParameters");
        return;
    }

    LOGW_WARN(Log::instance()->getLogger(), L"Running updater" << Utility::s2ws(updateFile.toStdString()).c_str());

    if (updateFile.endsWith(".exe")) {
        QProcess::startDetached(updateFile, QStringList() << "/S"
                                                          << "/launch");
    } else if (updateFile.endsWith(".msi")) {
        // When MSIs are installed without gui they cannot launch applications
        // as they lack the user context. That is why we need to run the client
        // manually here. We wrap the msiexec and client invocation in a powershell
        // script because kdrive.exe will be shut down for installation.
        // | Out-Null forces powershell to wait for msiexec to finish.
        auto preparePathForPowershell = [](QString path) {
            path.replace("'", "''");

            return QDir::toNativeSeparators(path);
        };

        // App support dir
        SyncPath msiLogPath(CommonUtility::getAppSupportDir());
        QString msiLogFile = SyncName2QStr((msiLogPath / "msi.log").native());
        QString command = QString("&{msiexec /norestart /passive /i '%1' /L*V '%2'| Out-Null ; &'%3'}")
                              .arg(preparePathForPowershell(updateFile), preparePathForPowershell(msiLogFile),
                                   preparePathForPowershell(QCoreApplication::applicationFilePath()));

        QProcess::startDetached("powershell.exe", QStringList{"-Command", command});
    } else {
        LOG_WARN(Log::instance()->getLogger(), "Unknown update file type or no update file availble.");
    }
}

void KDCUpdater::checkForUpdate() {
    QNetworkReply *reply = _accessManager->get(QNetworkRequest(_updateUrl));
    connect(_timeoutWatchdog, &QTimer::timeout, this, &KDCUpdater::slotTimedOut);
    _timeoutWatchdog->start(30 * 1000);
    connect(reply, &QNetworkReply::finished, this, &KDCUpdater::slotVersionInfoArrived);

    setDownloadState(CheckingServer);
}

void KDCUpdater::slotOpenUpdateUrl() {
    QDesktopServices::openUrl(_updateInfo.web());
}

bool KDCUpdater::updateSucceeded() const {
    qint64 targetVersionInt =
        Helper::stringVersionToInt(QString::fromStdString(ParametersCache::instance()->parameters().updateTargetVersion()));
    qint64 currentVersion = Helper::currentVersionToInt();
    return currentVersion >= targetVersionInt;
}

void KDCUpdater::slotVersionInfoArrived() {
    _timeoutWatchdog->stop();
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Failed to reach version check url: " << Utility::s2ws(reply->errorString().toStdString()).c_str());
        setDownloadState(KDCUpdater::Unknown);
        return;
    }

    QString xml = QString::fromUtf8(reply->readAll());

    bool ok = false;
    _updateInfo = UpdateInfo::parseString(xml, &ok);
    if (ok) {
        versionInfoArrived(_updateInfo);
    } else {
        LOG_WARN(Log::instance()->getLogger(), "Could not parse update information.");
        setDownloadState(KDCUpdater::Unknown);
    }
}

void KDCUpdater::slotTimedOut() {
    setDownloadState(DownloadTimedOut);
}

////////////////////////////////////////////////////////////////////////

NSISUpdater::NSISUpdater(const QUrl &url) : KDCUpdater(url) {}

void NSISUpdater::slotWriteFile() {
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (_file->isOpen()) {
        _file->write(reply->readAll());
    }
}

void NSISUpdater::wipeUpdateData() {
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

void NSISUpdater::getVersions(QString &targetVersion, QString &targetVersionString, QString &clientVersion) {
    targetVersion = QString::fromStdString(ParametersCache::instance()->parameters().updateTargetVersion());
    targetVersionString = QString::fromStdString(ParametersCache::instance()->parameters().updateTargetVersionString());
    clientVersion = UpdaterServer::clientVersion();
}

bool NSISUpdater::autoUpdateAttempted() {
    return ParametersCache::instance()->parameters().autoUpdateAttempted();
}

void NSISUpdater::slotDownloadFinished() {
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

void NSISUpdater::versionInfoArrived(const UpdateInfo &info) {
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
                connect(reply, &QIODevice::readyRead, this, &NSISUpdater::slotWriteFile);
                connect(reply, &QNetworkReply::finished, this, &NSISUpdater::slotDownloadFinished);
                setDownloadState(Downloading);
                _file.reset(new QTemporaryFile);
                _file->setAutoRemove(true);
                _file->open();
            }
        }
    }
}

void NSISUpdater::showNoUrlDialog(const UpdateInfo &info) {
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

    connect(skip, &QAbstractButton::clicked, this, &NSISUpdater::slotSetSeenVersion);
    connect(getupdate, &QAbstractButton::clicked, this, &NSISUpdater::slotOpenUpdateUrl);

    layout->addWidget(bb);

    msgBox->open();
}

bool NSISUpdater::handleStartup() {
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

void NSISUpdater::slotSetSeenVersion() {
    ParametersCache::instance()->parameters().setSeenVersion(updateInfo().version().toStdString());
    setDownloadState(LastVersionSkipped);
    ExitCode exitCode = ParametersCache::instance()->save();
    if (exitCode != ExitCodeOk) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParametersCache::saveParameters");
        return;
    }
}


void NSISUpdater::slotUnsetSeenVersion() {
    if (ParametersCache::instance()->parameters().seenVersion().empty()) {
        return;
    }
    ParametersCache::instance()->parameters().setSeenVersion(std::string());
    ExitCode exitCode = ParametersCache::instance()->save();
    if (exitCode != ExitCodeOk) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParametersCache::saveParameters");
        return;
    }
    checkForUpdate();
}
////////////////////////////////////////////////////////////////////////

PassiveUpdateNotifier::PassiveUpdateNotifier(const QUrl &url) : KDCUpdater(url) {}

void PassiveUpdateNotifier::versionInfoArrived(const UpdateInfo &info) {
    qint64 currentVer = Helper::currentVersionToInt();
    qint64 remoteVer = Helper::stringVersionToInt(info.version());

    if (info.version().isEmpty() || currentVer >= remoteVer) {
        LOG_INFO(Log::instance()->getLogger(), "Client is on latest version!");
        setDownloadState(UpToDate);
    } else {
        setDownloadState(UpdateOnlyAvailableThroughSystem);
    }
}

}  // namespace KDC
