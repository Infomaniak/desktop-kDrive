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

#include "clientgui.h"
#include "guiutility.h"
#include "custommessagebox.h"
#include "appclient.h"
#include "guirequests.h"
#include "parameterscache.h"
#include "libcommongui/utility/utility.h"
#include "libcommon/theme/theme.h"
#include "gui/updater/updatedialog.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QProcessEnvironment>

#if defined(Q_OS_MAC)
#include <objc/objc.h>
#include <objc/runtime.h>
#include <objc/message.h>
#endif

#define INIT_TRIALS 5

#define MAX_ERRORS_DISPLAYED 100

namespace KDC {

Q_LOGGING_CATEGORY(lcClientGui, "gui.clientgui", QtInfoMsg)

ClientGui::ClientGui(AppClient *parent) :
    QObject(),
    _app(parent) {
    connect(qGuiApp, &QGuiApplication::screenAdded, this, &ClientGui::onScreenUpdated);
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, &ClientGui::onScreenUpdated);

    connect(_app, &AppClient::userAdded, this, &ClientGui::onUserAdded);
    connect(_app, &AppClient::userUpdated, this, &ClientGui::onUserUpdated);
    connect(_app, &AppClient::userStatusChanged, this, &ClientGui::onUserStatusChanged);
    connect(_app, &AppClient::userRemoved, this, &ClientGui::onUserRemoved);

    connect(_app, &AppClient::accountAdded, this, &ClientGui::onAccountAdded);
    connect(_app, &AppClient::accountUpdated, this, &ClientGui::onAccountUpdated);
    connect(_app, &AppClient::accountRemoved, this, &ClientGui::onAccountRemoved);

    connect(_app, &AppClient::driveAdded, this, &ClientGui::onDriveAdded);
    connect(_app, &AppClient::driveUpdated, this, &ClientGui::onDriveUpdated);
    connect(_app, &AppClient::driveQuotaUpdated, this, &ClientGui::onDriveQuotaUpdated);
    connect(_app, &AppClient::driveRemoved, this, &ClientGui::onDriveRemoved);
    connect(_app, &AppClient::driveDeletionFailed, this, &ClientGui::onDriveDeletionFailed);

    connect(_app, &AppClient::syncAdded, this, &ClientGui::onSyncAdded);
    connect(_app, &AppClient::syncUpdated, this, &ClientGui::onSyncUpdated);
    connect(_app, &AppClient::syncRemoved, this, &ClientGui::onSyncRemoved);
    connect(_app, &AppClient::syncDeletionFailed, this, &ClientGui::onSyncDeletionFailed);
    connect(_app, &AppClient::syncProgressInfo, this, &ClientGui::onProgressInfo);
    connect(_app, &AppClient::itemCompleted, this, &ClientGui::itemCompleted);
    connect(_app, &AppClient::vfsConversionCompleted, this, &ClientGui::vfsConversionCompleted);
    connect(_app, &AppClient::newBigFolder, this, &ClientGui::newBigFolder);
    connect(_app, &AppClient::showNotification, this, &ClientGui::onShowOptionalTrayMessage);
    connect(_app, &AppClient::errorAdded, this, &ClientGui::onErrorAdded);
    connect(_app, &AppClient::errorsCleared, this, &ClientGui::onErrorsCleared);
    connect(_app, &AppClient::folderSizeCompleted, this, &ClientGui::folderSizeCompleted);
    connect(_app, &AppClient::fixConflictingFilesCompleted, this, &ClientGui::onFixConflictingFilesCompleted);
    connect(_app, &AppClient::updateStateChanged, this, &ClientGui::updateStateChanged);
    connect(_app, &AppClient::showWindowsUpdateDialog, this, &ClientGui::onShowWindowsUpdateDialog, Qt::QueuedConnection);

    connect(this, &ClientGui::refreshStatusNeeded, this, &ClientGui::onRefreshStatusNeeded);
    connect(this, &ClientGui::appVersionLocked, this, &ClientGui::onAppVersionLocked);

    connect(&_refreshErrorListTimer, &QTimer::timeout, this, &ClientGui::onRefreshErrorList);

    connect(_app, &AppClient::logUploadStatusUpdated, this, &ClientGui::logUploadStatusUpdated);

    _refreshErrorListTimer.setInterval(1 * 1000);
    _refreshErrorListTimer.start();
}

void ClientGui::init() {
    bool error = false;
    for (int trial = 1; trial <= INIT_TRIALS; trial++) {
        if (loadInfoMaps()) {
            qCDebug(lcClientGui()) << "loadInfoMaps succeeded";
            error = false;
            break;
        } else {
            qCWarning(lcClientGui()) << "loadInfoMaps failed for trial=" << trial;
            error = true;
            if (trial < INIT_TRIALS) {
                KDC::CommonGuiUtility::sleep(static_cast<unsigned long>(std::pow(2, trial)));
            }
        }
    }

    if (error) {
        CustomMessageBox msgBox(QMessageBox::Warning, tr("Unable to initialize kDrive client"), QMessageBox::Ok);
        msgBox.exec();
        qApp->quit();
        return;
    }

    resetSystray();
    setupSynthesisPopover();
    setupParametersDialog();

    // Refresh errors
    refreshErrorList(0);
    for (const auto &driveInfo: _driveInfoMap) {
        refreshErrorList(driveInfo.first);
    }

    // Refresh status
    ExitCode exitCode = GuiRequests::askForStatus();
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcClientGui()) << "Error in Requests::askForStatus";
    }

    emit syncListRefreshed();
}

bool ClientGui::isConnected() {
    return GuiRequests::isConnnected();
}

void ClientGui::onErrorAdded(bool serverLevel, ExitCode exitCode, int syncDbId) {
    if (exitCode == ExitCode::InvalidToken) {
        auto userIt = _userInfoMap.find(_currentUserDbId);
        if (userIt != _userInfoMap.end() && !userIt->second.credentialsAsked()) {
            userIt->second.setCredentialsAsked(true);
            if (_addDriveWizard) {
                emit _addDriveWizard->exit();
            }
            _app->askUserToLoginAgain(_currentUserDbId, userIt->second.email(), true);
        }
    }

    // Refresh errorlist
    if (serverLevel) {
        refreshErrorList(0);
    } else {
        const auto syncInfoMapIt = _syncInfoMap.find(syncDbId);
        if (syncInfoMapIt == _syncInfoMap.end()) {
            qCWarning(lcClientGui()) << "Sync not found in sync map for syncDbId=" << syncDbId;
            return;
        }
        refreshErrorList(syncInfoMapIt->second.driveDbId());
    }
}

void ClientGui::onErrorsCleared(int syncDbId) {
    // Refresh errorlist
    if (syncDbId == 0) {
        refreshErrorList(0);
    } else {
        const auto syncInfoMapIt = _syncInfoMap.find(syncDbId);
        if (syncInfoMapIt == _syncInfoMap.end()) {
            qCWarning(lcClientGui()) << "Sync not found in sync map for syncDbId=" << syncDbId;
            return;
        }
        refreshErrorList(syncInfoMapIt->second.driveDbId());
    }
}

void ClientGui::onFixConflictingFilesCompleted(int syncDbId, uint64_t nbErrors) {
    if (nbErrors > 0) {
        const auto syncInfoMapIt = _syncInfoMap.find(syncDbId);
        if (syncInfoMapIt != _syncInfoMap.end()) {
            KDC::CustomMessageBox msgBox(QMessageBox::Warning,
                                         tr("Failed to fix conflict(s) on %1 item(s) in sync folder: %2")
                                                 .arg(nbErrors)
                                                 .arg(syncInfoMapIt->second.localPath()),
                                         QMessageBox::Ok);
            msgBox.exec();
        }
    }

    onErrorsCleared(syncDbId);
}

// void ClientGui::onActionSynthesisTriggered(bool) {
//     showSynthesisDialog();
// }

void ClientGui::onActionPreferencesTriggered(bool) {
    onShowParametersDialog();
}

void ClientGui::computeOverallSyncStatus() {
    bool allDisconnected = true;
    bool allPaused = true;
    for (const auto &[_, userInfo]: _userInfoMap) {
        if (userInfo.connected()) {
            allDisconnected = false;
            break;
        }
    }
    if (allDisconnected) {
        (void) GuiRequests::updateSystray(SyncStatus::PauseAsked, tr("Please sign in"));
        return;
    }
    for (const auto &syncInfoMapIt: _syncInfoMap) {
        if (!syncInfoMapIt.second.paused()) {
            allPaused = false;
            break;
        }
    }
    if (allPaused) {
        (void) GuiRequests::updateSystray(SyncStatus::PauseAsked, tr("Synchronization is paused"));
        return;
    }
    // display the info of the least successful sync (eg. do not just display the result of the latest sync)
    SyncStatus overallStatus = SyncStatus::Undefined;
    bool hasUnresolvedConflicts = false;
    computeTrayOverallStatus(overallStatus, hasUnresolvedConflicts);
    // If the sync succeeded but there are unresolved conflicts, show the problem icon!
    auto iconStatus = overallStatus;
    if (iconStatus == SyncStatus::Idle && hasUnresolvedConflicts) {
        iconStatus = SyncStatus::Error;
    }
    // If we don't get a status for whatever reason, that's a Problem
    if (iconStatus == SyncStatus::Undefined) {
        iconStatus = SyncStatus::Error;
    }
    // create the tray blob message, check if we have an defined state
    QString trayMessage;
    if (!_syncInfoMap.empty()) {
#ifdef Q_OS_WIN
        // Windows has a 128-char tray tooltip length limit.
        trayMessage = trayTooltipStatusString(overallStatus, hasUnresolvedConflicts, false);
#else
        QStringList allStatusStrings;
        for (const auto &syncInfoMapIt: _syncInfoMap) {
            QString syncMessage = trayTooltipStatusString(
                    syncInfoMapIt.second.status(), syncInfoMapIt.second.unresolvedConflicts(), syncInfoMapIt.second.paused());

            QString shortLocalPath = shortGuiLocalPath(syncInfoMapIt.second.localPath());
            allStatusStrings += tr("Folder %1: %2").arg(shortLocalPath, syncMessage);
        }
        trayMessage = allStatusStrings.join(QLatin1String("\n"));
#endif
    } else {
        trayMessage = tr("There are no sync folders configured.");
    }
    (void) GuiRequests::updateSystray(iconStatus, trayMessage);
}

void ClientGui::hideAndShowTray() {
    // if (!_tray) {
    //     qCDebug(lcClientGui) << "Tray not ready";
    //     return;
    // }
    //
    // _tray->hide();
    // _tray->show();
}

void ClientGui::showSynthesisDialog(const QRect &geometry) {
    if (_synthesisPopover) {
        if (_synthesisPopover->isVisible()) {
            _synthesisPopover->done(QDialog::Accepted);
        } else {
            QRect trayIconRect = geometry;
            if (!trayIconRect.isValid()) {
                trayIconRect = QRect(QCursor::pos(), QSize(0, 0));
            }
            _synthesisPopover->setPosition(trayIconRect);
            raiseDialog(_synthesisPopover.get());
        }
        _synthesisPopover->refreshLockedStatus();
    }
}

int ClientGui::driveErrorsCount(int driveDbId, bool unresolved) const {
    const auto driveInfoMapIt = _driveInfoMap.find(driveDbId);
    if (driveInfoMapIt == _driveInfoMap.end()) {
        qCDebug(lcClientGui()) << "Drive not found in drive map!";
        return 0;
    }

    return unresolved ? driveInfoMapIt->second.unresolvedErrorsCount() : driveInfoMapIt->second.autoresolvedErrorsCount();
}

const QString ClientGui::folderPath(int syncDbId, const QString &filePath) const {
    QString fullFilePath;
    const auto syncInfoIt = _syncInfoMap.find(syncDbId);
    if (syncInfoIt != _syncInfoMap.end()) {
        fullFilePath = syncInfoIt->second.localPath() + dirSeparator + filePath;
    }

    return fullFilePath;
}

bool ClientGui::setCurrentUserDbId(int userDbId) {
    const auto &userInfoMapIt = _userInfoMap.find(userDbId);
    if (userInfoMapIt == _userInfoMap.end()) {
        qCWarning(lcClientGui()) << "User not found in user map!";
        return false;
    }
    _currentUserDbId = userDbId;

    // Set 1st account linked to user as current
    _currentAccountDbId = 0;
    for (const auto &accountInfoMapElt: _accountInfoMap) {
        if (accountInfoMapElt.second.userDbId() == userDbId) {
            _currentAccountDbId = accountInfoMapElt.first;
            break;
        }
    }

    if (_currentAccountDbId) {
        // Set 1st drive linked to account as current
        _currentDriveDbId = 0;
        for (const auto &driveInfoMapElt: _driveInfoMap) {
            if (driveInfoMapElt.second.accountDbId() == _currentAccountDbId) {
                _currentAccountDbId = driveInfoMapElt.first;
                break;
            }
        }
    }

    return true;
}

bool ClientGui::setCurrentAccountDbId(int accountDbId) {
    const auto &accountInfoMapIt = _accountInfoMap.find(accountDbId);
    if (accountInfoMapIt == _accountInfoMap.end()) {
        qCWarning(lcClientGui()) << "Account not found in account map!";
        return false;
    }
    if (_currentUserDbId != accountInfoMapIt->second.userDbId()) {
        if (!setCurrentUserDbId(accountInfoMapIt->second.userDbId())) {
            qCWarning(lcClientGui()) << "Error in setCurrentUserDbId!";
            return false;
        }
    }
    _currentAccountDbId = accountDbId;

    // Set 1st drive linked to account as current
    _currentDriveDbId = 0;
    for (const auto &driveInfoMapElt: _driveInfoMap) {
        if (driveInfoMapElt.second.accountDbId() == accountDbId) {
            _currentDriveDbId = driveInfoMapElt.first;
            break;
        }
    }

    return true;
}

bool ClientGui::setCurrentDriveDbId(int driveDbId) {
    const auto &driveInfoMapIt = _driveInfoMap.find(driveDbId);
    if (driveInfoMapIt == _driveInfoMap.end()) {
        qCWarning(lcClientGui()) << "Drive not found in drive map!";
        return false;
    }
    if (_currentAccountDbId != driveInfoMapIt->second.accountDbId()) {
        if (!setCurrentAccountDbId(driveInfoMapIt->second.accountDbId())) {
            qCWarning(lcClientGui()) << "Error in setCurrentAccountDbId!";
            return false;
        }
    }
    _currentDriveDbId = driveDbId;

    return true;
}

void ClientGui::loadSyncInfoMap(int driveDbId, std::map<int, SyncInfoClient> &syncInfoMap) {
    syncInfoMap.clear();
    for (const auto &syncInfoMapElt: _syncInfoMap) {
        if (syncInfoMapElt.second.driveDbId() == driveDbId) {
            syncInfoMap.insert({syncInfoMapElt.first, syncInfoMapElt.second});
        }
    }
}

void ClientGui::setupSynthesisPopover() {
    if (_synthesisPopover) {
        return;
    }

    _synthesisPopover.reset(new SynthesisPopover(shared_from_this(), _app->debugCrash()));
    connect(_synthesisPopover.get(), &SynthesisPopover::showParametersDialog, this, &ClientGui::onShowParametersDialog);
    connect(_synthesisPopover.get(), &SynthesisPopover::exit, _app, &AppClient::onQuit);
    connect(_synthesisPopover.get(), &SynthesisPopover::addDrive, this, &ClientGui::onNewDriveWizard);
    connect(_synthesisPopover.get(), &SynthesisPopover::disableNotifications, this, &ClientGui::onDisableNotifications);
    connect(_synthesisPopover.get(), &SynthesisPopover::applyStyle, this, &ClientGui::onApplyStyle);
    connect(_synthesisPopover.get(), &SynthesisPopover::crash, _app, &AppClient::onCrash);
    connect(_synthesisPopover.get(), &SynthesisPopover::crashServer, _app, &AppClient::onCrashServer);
    connect(_synthesisPopover.get(), &SynthesisPopover::crashEnforce, _app, &AppClient::onCrashEnforce);
    connect(_synthesisPopover.get(), &SynthesisPopover::crashFatal, _app, &AppClient::onCrashFatal);
    connect(_synthesisPopover.get(), &SynthesisPopover::executeSyncAction, this, &ClientGui::onExecuteSyncAction);

    connect(this, &ClientGui::userListRefreshed, _synthesisPopover.get(), &SynthesisPopover::onConfigRefreshed);
    connect(this, &ClientGui::accountListRefreshed, _synthesisPopover.get(), &SynthesisPopover::onConfigRefreshed);
    connect(this, &ClientGui::driveListRefreshed, _synthesisPopover.get(), &SynthesisPopover::onConfigRefreshed);
    connect(this, &ClientGui::syncListRefreshed, _synthesisPopover.get(), &SynthesisPopover::onConfigRefreshed);
    connect(this, &ClientGui::updateProgress, _synthesisPopover.get(), &SynthesisPopover::onUpdateProgress);
    connect(this, &ClientGui::driveQuotaUpdated, _synthesisPopover.get(), &SynthesisPopover::onDriveQuotaUpdated);
    connect(this, &ClientGui::itemCompleted, _synthesisPopover.get(), &SynthesisPopover::onItemCompleted);
    connect(this, &ClientGui::errorAdded, _synthesisPopover.get(), &SynthesisPopover::onRefreshErrorList);
    connect(this, &ClientGui::errorsCleared, _synthesisPopover.get(), &SynthesisPopover::onRefreshErrorList);
    connect(this, &ClientGui::appVersionLocked, _synthesisPopover.get(), &SynthesisPopover::onAppVersionLocked);
    connect(this, &ClientGui::refreshStatusNeeded, _synthesisPopover.get(), &SynthesisPopover::onRefreshStatusNeeded);

    // This is an old compound flag that people might still depend on
    bool qdbusmenuWorkarounds = false;
    if (qdbusmenuWorkarounds) {
        _workaroundFakeDoubleClick = true;
        _workaroundNoAboutToShowUpdate = true;
        _workaroundShowAndHideTray = true;
    }

#ifdef Q_OS_MAC
    // https://bugreports.qt.io/browse/QTBUG-54633
    _workaroundNoAboutToShowUpdate = true;
    _workaroundManualVisibility = true;
#endif

    qCInfo(lcClientGui) << "Tray menu workarounds:"
                        << "noabouttoshow:" << _workaroundNoAboutToShowUpdate << "fakedoubleclick:" << _workaroundFakeDoubleClick
                        << "showhide:" << _workaroundShowAndHideTray << "manualvisibility:" << _workaroundManualVisibility;

    connect(&_delayedTrayUpdateTimer, &QTimer::timeout, this, &ClientGui::onUpdateSystray);
    _delayedTrayUpdateTimer.setInterval(2 * 1000);
    _delayedTrayUpdateTimer.setSingleShot(true);

    // Populate the context menu now.
    onUpdateSystray();
}

void ClientGui::setupParametersDialog() {
    _parametersDialog.reset(new ParametersDialog(shared_from_this()));
    connect(_parametersDialog.get(), &ParametersDialog::addDrive, this, &ClientGui::onNewDriveWizard);
    connect(_parametersDialog.get(), &ParametersDialog::setStyle, this, &ClientGui::onSetStyle);
    connect(_parametersDialog.get(), &ParametersDialog::removeDrive, this, &ClientGui::onRemoveDrive);
    connect(_parametersDialog.get(), &ParametersDialog::removeSync, this, &ClientGui::onRemoveSync);
    connect(_parametersDialog.get(), &ParametersDialog::executeSyncAction, this, &ClientGui::onExecuteSyncAction);

    connect(this, &ClientGui::userListRefreshed, _parametersDialog.get(), &ParametersDialog::onConfigRefreshed);
    connect(this, &ClientGui::accountListRefreshed, _parametersDialog.get(), &ParametersDialog::onConfigRefreshed);
    connect(this, &ClientGui::driveListRefreshed, _parametersDialog.get(), &ParametersDialog::onConfigRefreshed);
    connect(this, &ClientGui::syncListRefreshed, _parametersDialog.get(), &ParametersDialog::onConfigRefreshed);
    connect(this, &ClientGui::updateProgress, _parametersDialog.get(), &ParametersDialog::onUpdateProgress);
    connect(this, &ClientGui::driveQuotaUpdated, _parametersDialog.get(), &ParametersDialog::onDriveQuotaUpdated);
    connect(this, &ClientGui::itemCompleted, _parametersDialog.get(), &ParametersDialog::onItemCompleted);
    connect(this, &ClientGui::newBigFolder, _parametersDialog.get(), &ParametersDialog::newBigFolder);
    connect(this, &ClientGui::errorAdded, _parametersDialog.get(), &ParametersDialog::onRefreshErrorList);
    connect(this, &ClientGui::refreshStatusNeeded, _parametersDialog.get(), &ParametersDialog::onRefreshStatusNeeded);
}

void ClientGui::onUpdateSystray() {
    // if (!_tray) {
    //     qCDebug(lcClientGui) << "Tray not ready";
    //     return;
    // }
    //
    // if (_workaroundShowAndHideTray) {
    //     // To make tray menu updates work with these bugs (see setupPopover)
    //     // we need to hide and show the tray icon. We don't want to do that
    //     // while it's visible!
    //     if (!_delayedTrayUpdateTimer.isActive()) {
    //         _delayedTrayUpdateTimer.start();
    //     }
    //     _tray->hide();
    // }
    //
    // if (_workaroundShowAndHideTray) {
    //     _tray->show();
    // }
}

void ClientGui::updateSystrayNeeded() {
    // if it's visible and we can update live: update now
    onUpdateSystray();
    return;
}

void ClientGui::resetSystray(bool currentVersionLocked) {
    //     (void) currentVersionLocked;
    //     _tray.reset(new Systray());
    //     _tray->setParent(this);
    //
    //     // for the beginning, set the offline icon until the account was verified
    //     QIcon testIcon = Theme::instance()->folderOfflineIcon(/*systray?*/ true, /*currently visible?*/ false);
    //     _tray->setIcon(testIcon);
    //
    //     if (_tray->geometry().width() == 0) {
    //         _tray->setContextMenu(new QMenu());
    // #ifdef Q_OS_LINUX
    //         if (osRequireMenuTray()) {
    //             _actionSynthesis =
    //                     _tray->contextMenu()->addAction(QIcon(":/client/resources/icons/actions/information.svg"),
    //                     QString());
    //             if (!currentVersionLocked) {
    //                 _actionPreferences =
    //                         _tray->contextMenu()->addAction(QIcon(":/client/resources/icons/actions/parameters.svg"),
    //                         QString());
    //             }
    //             _tray->contextMenu()->addSeparator();
    //             _actionQuit = _tray->contextMenu()->addAction(QIcon(":/client/resources/icons/actions/error-sync.svg"),
    //             QString());
    //         }
    //
    // #endif
    //     }
    //
    //     if (!_tray->contextMenu() || _tray->contextMenu()->isEmpty()) {
    //         connect(_tray.get(), &QSystemTrayIcon::activated, this, &ClientGui::onTrayClicked);
    //     }
    // #ifdef Q_OS_LINUX
    //     else {
    //         connect(_tray->contextMenu(), &QMenu::aboutToShow, this, &ClientGui::retranslateUi);
    //
    //         if (_actionSynthesis) connect(_actionSynthesis, &QAction::triggered, this,
    //         &ClientGui::onActionSynthesisTriggered); if (_actionQuit) connect(_actionQuit, &QAction::triggered, _app,
    //         &AppClient::onQuit); if (_actionPreferences && !currentVersionLocked)
    //             connect(_actionPreferences, &QAction::triggered, this, &ClientGui::onActionPreferencesTriggered);
    //     }
    // #endif
    //
    //     _tray->show();
}

QString ClientGui::shortGuiLocalPath(const QString &path) {
    QString home = QDir::homePath();
    if (!home.endsWith('/')) {
        home.append('/');
    }
    QString shortPath(path);
    if (shortPath.startsWith(home)) {
        shortPath = shortPath.mid(home.length());
    }
    if (shortPath.length() > 1 && shortPath.endsWith('/')) {
        shortPath.chop(1);
    }
    return QDir::toNativeSeparators(shortPath);
}

void ClientGui::computeTrayOverallStatus(SyncStatus &status, bool &unresolvedConflicts) {
    status = SyncStatus::Undefined;
    unresolvedConflicts = false;

    // if one folder: show the state of the one folder.
    // if more folders:
    // if one of them has an error -> show error
    // if one is paused, but others ok, show ok
    size_t cnt = _syncInfoMap.size();
    if (cnt == 1) {
        const auto &syncInfoMapIt = _syncInfoMap.begin();
        if (syncInfoMapIt->second.paused()) {
            status = SyncStatus::Paused;
        } else {
            status = syncInfoMapIt->second.status();
            if (status == SyncStatus::Undefined) {
                status = SyncStatus::Error;
            }
        }
        unresolvedConflicts = syncInfoMapIt->second.unresolvedConflicts();
    } else {
        unsigned int errorsSeen = 0;
        unsigned int idleSeen = 0;
        unsigned int abortOrPausedSeen = 0;
        unsigned int runSeen = 0;

        for (const auto &syncInfoMapIt: _syncInfoMap) {
            if (syncInfoMapIt.second.paused()) {
                abortOrPausedSeen++;
            } else {
                switch (syncInfoMapIt.second.status()) {
                    case SyncStatus::Running:
                        runSeen++;
                        break;
                    case SyncStatus::Idle:
                        idleSeen++;
                        break;
                    case SyncStatus::Error:
                        errorsSeen++;
                        break;
                    case SyncStatus::PauseAsked:
                    case SyncStatus::Paused:
                    case SyncStatus::StopAsked:
                    case SyncStatus::Stopped:
                        abortOrPausedSeen++;
                        break;
                    case SyncStatus::Undefined:
                    case SyncStatus::Starting:
                        break;
                    case SyncStatus::EnumEnd: {
                        assert(false && "Invalid enum value in switch statement.");
                    }
                }
            }
            if (syncInfoMapIt.second.unresolvedConflicts()) {
                unresolvedConflicts = true;
            }
        }
        if (errorsSeen > 0) {
            status = SyncStatus::Error;
        } else if (abortOrPausedSeen > 0 && abortOrPausedSeen == cnt) {
            // only if all folders are paused
            status = SyncStatus::Paused;
        } else if (runSeen > 0) {
            status = SyncStatus::Running;
        } else if (idleSeen > 0) {
            status = SyncStatus::Idle;
        }
    }
    if (_generalErrorsCounter) {
        status = SyncStatus::Error;
    }
}

QString ClientGui::trayTooltipStatusString(SyncStatus status, bool unresolvedConflicts, bool paused) {
    QString statusString;
    switch (status) {
        case SyncStatus::Undefined:
            statusString = tr("Undefined State.");
            break;
        case SyncStatus::Starting:
            statusString = tr("Waiting to start syncing.");
            break;
        case SyncStatus::Running:
            statusString = tr("Sync is running.");
            break;
        case SyncStatus::Idle:
            if (unresolvedConflicts) {
                statusString = tr("Sync was successful, unresolved conflicts.");
            } else {
                statusString = tr("Last Sync was successful.");
            }
            break;
        case SyncStatus::Error:
            break;
        case SyncStatus::StopAsked:
        case SyncStatus::Stopped:
            statusString = tr("User Abort.");
            break;
        case SyncStatus::PauseAsked:
        case SyncStatus::Paused:
            statusString = tr("Sync is paused.");
            break;
            // no default case on purpose, check compiler warnings
        case SyncStatus::EnumEnd: {
            assert(false && "Invalid enum value in switch statement.");
        }
    }
    if (paused) {
        // sync is disabled.
        statusString = tr("%1 (Sync is paused)").arg(statusString);
    }
    return statusString;
}

void ClientGui::executeSyncAction(ActionType type, int syncDbId) {
    auto syncInfoMapIt = _syncInfoMap.find(syncDbId);
    if (syncInfoMapIt == _syncInfoMap.end()) {
        qCWarning(lcClientGui()) << "Sync not found in syncInfoMap for syncDbId=" << syncDbId;
        return;
    }

    auto currentStatus = syncInfoMapIt->second.status();
    ExitCode exitCode;
    switch (type) {
        case ActionType::Stop:
            if (currentStatus == SyncStatus::Undefined || currentStatus == SyncStatus::PauseAsked ||
                currentStatus == SyncStatus::Paused || currentStatus == SyncStatus::StopAsked ||
                currentStatus == SyncStatus::Stopped || currentStatus == SyncStatus::Error) {
                return;
            }

            syncInfoMapIt->second.setStatus(SyncStatus::PauseAsked);
            emit updateProgress(syncDbId);

            exitCode = GuiRequests::syncStop(syncDbId);
            if (exitCode != ExitCode::Ok) {
                qCWarning(lcClientGui()) << "Error in Requests::syncStop for syncDbId=" << syncDbId << " code=" << exitCode;
            }
            break;
        case ActionType::Start:
            if (currentStatus == SyncStatus::Undefined || currentStatus == SyncStatus::Idle ||
                currentStatus == SyncStatus::Running || currentStatus == SyncStatus::Starting) {
                return;
            }

            syncInfoMapIt->second.setStatus(SyncStatus::Starting);
            emit updateProgress(syncDbId);

            exitCode = GuiRequests::syncStart(syncDbId);
            if (exitCode != ExitCode::Ok) {
                qCWarning(lcClientGui()) << "Error in Requests::syncStart for syncDbId=" << syncDbId << " code=" << exitCode;
                syncInfoMapIt->second.setStatus(SyncStatus::Paused);
                emit updateProgress(syncDbId);
            }
            break;
        case ActionType::EnumEnd: {
            assert(false && "Invalid enum value in switch statement.");
        }
    }
}

void ClientGui::refreshErrorList(int driveDbId) {
    if (!_driveWithNewErrorSet.contains(driveDbId)) {
        _driveWithNewErrorSet.insert(driveDbId);
    }
}

void ClientGui::onShowTrayMessage(const QString &title, const QString &msg) {
    // if (_tray) {
    //     _tray->showMessage(title, msg);
    // } else {
    //     qCWarning(lcClientGui) << "Tray not ready: " << msg;
    // }
}

void ClientGui::onShowOptionalTrayMessage(const QString &title, const QString &msg) {
    if (ParametersCache::instance()->parametersInfo().notificationsDisabled() != NotificationsDisabled::Never) {
        if (_notificationEnableDate != QDateTime() && _notificationEnableDate > QDateTime::currentDateTime()) {
            return;
        }
    }

    onShowTrayMessage(title, msg);
}

void ClientGui::onNewDriveWizard() {
    if (!_addDriveWizard) {
        _addDriveWizard.reset(new KDC::AddDriveWizard(shared_from_this(), _currentUserDbId));
        _addDriveWizard->setAttribute(Qt::WA_DeleteOnClose);

        connect(_addDriveWizard.get(), &QDialog::accepted, this, &ClientGui::onAddDriveAccepted);
        connect(_addDriveWizard.get(), &QDialog::rejected, this, &ClientGui::onAddDriveRejected);
        connect(_addDriveWizard.get(), &QDialog::destroyed, this, &ClientGui::onAddDriveFinished);

        _addDriveWizard->show();
    }

    raiseDialog(_addDriveWizard.get());
}


void ClientGui::onShowWindowsUpdateDialog(const VersionInfo &versionInfo) const {
    static std::mutex mutex;
    const std::unique_lock lock(mutex, std::try_to_lock);
    if (!lock.owns_lock()) return;
    if (UpdateDialog dialog(versionInfo); dialog.exec() == QDialog::Accepted) {
        GuiRequests::startInstaller();
    } else if (dialog.skip()) {
        GuiRequests::skipUpdate(versionInfo.fullVersion());
    }
}

void ClientGui::onDisableNotifications(NotificationsDisabled type, QDateTime value) {
    ParametersCache::instance()->parametersInfo().setNotificationsDisabled(type);
    ParametersCache::instance()->saveParametersInfo();

    if (type == NotificationsDisabled::Never) {
        _notificationEnableDate = QDateTime();
    } else if (type == NotificationsDisabled::Always) {
        _notificationEnableDate = QDateTime();
    } else {
        _notificationEnableDate = value;
    }
}

void ClientGui::onApplyStyle() {
    KDC::GuiUtility::setStyle(qApp);
}

void ClientGui::onSetStyle(bool darkTheme) {
    ParametersCache::instance()->parametersInfo().setDarkTheme(darkTheme);
    KDC::GuiUtility::setStyle(qApp, darkTheme);

    // Force apply style
    _parametersDialog->forceRedraw();
    _synthesisPopover->forceRedraw();
}

void ClientGui::onAddDriveAccepted() {
    AppClient *app = qobject_cast<AppClient *>(qApp);
    app->onWizardDone(QDialog::Accepted);

    // Run next action
    switch (_addDriveWizard->nextAction()) {
        case KDC::GuiUtility::WizardAction::OpenFolder:
            if (!KDC::GuiUtility::openFolder(_addDriveWizard->localFolderPath())) {
                KDC::CustomMessageBox msgBox(QMessageBox::Warning,
                                             tr("Unable to open folder path %1.").arg(_addDriveWizard->localFolderPath()),
                                             QMessageBox::Ok);
                msgBox.exec();
            }
            break;
        case KDC::GuiUtility::WizardAction::OpenParameters:
            onShowParametersDialog(_addDriveWizard->syncDbId());
            break;
        case KDC::GuiUtility::WizardAction::AddDrive:
            QTimer::singleShot(100, [this]() { onNewDriveWizard(); });
            break;
    }
}

void ClientGui::onAddDriveRejected() {
    AppClient *app = qobject_cast<AppClient *>(qApp);
    app->onWizardDone(QDialog::Rejected);
}

void ClientGui::onAddDriveFinished() {
    static_cast<void>(_addDriveWizard.release());
}

void ClientGui::onCopyLinkItem(int driveDbId, const QString &nodeId) {
    QString linkUrl;
    ExitCode exitCode = GuiRequests::getPublicLinkUrl(driveDbId, nodeId, linkUrl);
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcClientGui()) << "Error in Requests::getPublicLinkUrl";
        return;
    }

    QApplication::clipboard()->setText(linkUrl);
    CustomMessageBox msgBox(QMessageBox::Information, tr("The shared link has been copied to the clipboard."), QMessageBox::Ok,
                            nullptr);
    msgBox.exec();
}

void ClientGui::onOpenWebviewItem(int driveDbId, const QString &nodeId) {
    QString linkUrl;
    ExitCode exitCode = GuiRequests::getPrivateLinkUrl(driveDbId, nodeId, linkUrl);
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcClientGui()) << "Error in Requests::getPrivateLinkUrl";
        return;
    }

    KDC::GuiUtility::openBrowser(linkUrl, nullptr);
}

void ClientGui::getWebviewDriveLink(int driveDbId, QString &driveLink) {
    ExitCode exitCode = GuiRequests::getPrivateLinkUrl(driveDbId, "", driveLink);
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcClientGui()) << "Error in Requests::getPrivateLinkUrl";
        return;
    }
}

void ClientGui::errorInfoList(int driveDbId, QList<ErrorInfo> &errorInfoList) {
    if (_errorInfoMap.find(driveDbId) != _errorInfoMap.end()) {
        errorInfoList = _errorInfoMap[driveDbId];
    }
}

void ClientGui::resolveConflictErrors(int driveDbId, bool keepLocalVersion) {
    ExitCode exitCode = GuiRequests::resolveConflictErrors(driveDbId, keepLocalVersion);
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcClientGui()) << "Error in Requests::resolveConflictErrors";
        return;
    }
}

void ClientGui::resolveUnsupportedCharErrors(int driveDbId) {
    ExitCode exitCode = GuiRequests::resolveUnsupportedCharErrors(driveDbId);
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcClientGui()) << "Error in Requests::resolveUnsupportedCharErrors";
        return;
    }
}

void ClientGui::onCopyUrlToClipboard(const QString &url) {
    QApplication::clipboard()->setText(url);
    CustomMessageBox msgBox(QMessageBox::Information, tr("The shared link has been copied to the clipboard."), QMessageBox::Ok,
                            nullptr);
    msgBox.exec();
}

void ClientGui::onScreenUpdated(QScreen *screen) {
    Q_UNUSED(screen)

    _synthesisPopover->hide();
    emit refreshStatusNeeded();
}

ExitCode ClientGui::loadError(int driveDbId, int syncDbId, ErrorLevel level) {
    const ExitCode exitCode = GuiRequests::getErrorInfoList(level, syncDbId, MAX_ERRORS_DISPLAYED, _errorInfoMap[driveDbId]);
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcClientGui()) << "Error in Requests::getErrorInfoList for level=" << level;
    }

    return exitCode;
}


void ClientGui::onRefreshErrorList() {
    if (_driveWithNewErrorSet.count()) {
        emit refreshStatusNeeded();
    }

    bool versionLocked = false;
    // Server level errors.
    if (_driveWithNewErrorSet.contains(0)) {
        _errorInfoMap[0].clear();
        if (ExitCode::Ok != ClientGui::loadError(0, 0, ErrorLevel::Server)) {
            return;
        }

        _generalErrorsCounter = (int) _errorInfoMap[0].count();
        emit errorAdded(0);
        for (const auto &errorInfo: _errorInfoMap[0]) {
            versionLocked = versionLocked || errorInfo.exitCode() == ExitCode::UpdateRequired;
        }

        _driveWithNewErrorSet.remove(0);
    }

    // Drive level errors (SyncPal or Node).
    for (auto it = _driveWithNewErrorSet.begin(); it != _driveWithNewErrorSet.end();) {
        const int driveDbId = *it;
        _errorInfoMap[driveDbId].clear();

        const auto driveInfoMapIt = _driveInfoMap.find(driveDbId);
        if (driveInfoMapIt == _driveInfoMap.end()) {
            qCWarning(lcClientGui()) << "Drive not found in drive map for driveDbId=" << driveDbId;
            return;
        }

        for (const auto &[syncDbId, syncInfo]: _syncInfoMap) {
            if (syncInfo.driveDbId() != driveDbId) continue;
            for (auto level: std::vector<ErrorLevel>{ErrorLevel::SyncPal, ErrorLevel::Node}) {
                if (ExitCode::Ok != loadError(driveDbId, syncDbId, level)) return;
            }
        }

        int unresolvedErrorsCount = 0;
        int autoresolvedErrorsCount = 0;
        for (const auto &errorInfo: _errorInfoMap[driveDbId]) {
            versionLocked = versionLocked || errorInfo.exitCode() == ExitCode::UpdateRequired;

            if (errorInfo.autoResolved()) {
                ++autoresolvedErrorsCount;
            } else {
                ++unresolvedErrorsCount;
            }
        }
        driveInfoMapIt->second.setUnresolvedErrorsCount(unresolvedErrorsCount);
        driveInfoMapIt->second.setAutoresolvedErrorsCount(autoresolvedErrorsCount);
        emit errorAdded(driveDbId);

        it = _driveWithNewErrorSet.erase(it);
    }

    if (versionLocked) emit appVersionLocked(versionLocked);
}

void ClientGui::closeAllExcept(const QWidget *exceptWidget) {
    std::vector<QDialog *> dialogs;
    dialogs.push_back(_synthesisPopover.get());
    dialogs.push_back(_parametersDialog.get());
    dialogs.push_back(_addDriveWizard.get());
    dialogs.push_back(_loginDialog.get());

    for (auto &dialog: dialogs) {
        if (dialog && exceptWidget != dialog) {
            dialog->hide();
        }
    }
}

bool ClientGui::isUserUsed(int userDbId) const {
    for (const auto &[accountDbId, accountInfoClient]: _accountInfoMap) {
        if (accountInfoClient.userDbId() == userDbId) {
            for (const auto &[driveDbId, driveInfoClient]: _driveInfoMap) {
                if (driveInfoClient.accountDbId() == accountDbId) {
                    for (const auto &[syncDbId, syncInfoClient]: _syncInfoMap) {
                        if (syncInfoClient.driveDbId() == driveDbId) {
                            return true;
                        }
                    }
                }
            }
        }
    }
    return false;
}

void ClientGui::onAppVersionLocked(bool currentVersionLocked) {
#ifdef Q_OS_LINUX
    resetSystray(currentVersionLocked);
#else
    (void) currentVersionLocked;
#endif
}

void ClientGui::onUserAdded(const UserInfo &userInfo) {
    _userInfoMap.insert({userInfo.dbId(), UserInfoClient(userInfo)});

    if (!_currentUserDbId) {
        _currentUserDbId = userInfo.dbId();
    }

    emit userListRefreshed();
    emit refreshStatusNeeded();
}

void ClientGui::onRemoveUser(int userDbId) {
    ExitCode exitCode = GuiRequests::deleteUser(userDbId);
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcClientGui()) << "Error in Requests::deleteUser for userDbId=" << userDbId;
        return;
    }
}

void ClientGui::onUserUpdated(const UserInfo &userInfo) {
    const auto userInfoMapIt = _userInfoMap.find(userInfo.dbId());
    if (userInfoMapIt != _userInfoMap.end()) {
        userInfoMapIt->second.setName(userInfo.name());
        userInfoMapIt->second.setEmail(userInfo.email());
        userInfoMapIt->second.setAvatar(userInfo.avatar());
        userInfoMapIt->second.setConnected(userInfo.connected());
        if (userInfo.connected()) {
            userInfoMapIt->second.setCredentialsAsked(false);
        }
        userInfoMapIt->second.setIsStaff(userInfo.isStaff());
        emit userListRefreshed();
    }
}

void ClientGui::onUserStatusChanged(int userDbId, bool connected, QString connexionError) {
    const auto userInfoMapIt = _userInfoMap.find(userDbId);
    if (userInfoMapIt != _userInfoMap.end()) {
        userInfoMapIt->second.setConnected(connected);
        userInfoMapIt->second.setConnectionError(connexionError);
        if (connected) {
            userInfoMapIt->second.setCredentialsAsked(false);
        }

        emit refreshStatusNeeded();
    }
}

void ClientGui::onUserRemoved(int userDbId) {
    auto userInfoMapIt = _userInfoMap.find(userDbId);
    if (userInfoMapIt != _userInfoMap.end()) {
        auto accountInfoMapIt = _accountInfoMap.begin();
        while (accountInfoMapIt != _accountInfoMap.end()) {
            // Erase accounts linked
            if (accountInfoMapIt->second.userDbId() == userDbId) {
                auto driveInfoMapIt = _driveInfoMap.begin();
                while (driveInfoMapIt != _driveInfoMap.end()) {
                    // Erase drives linked
                    if (driveInfoMapIt->second.accountDbId() == accountInfoMapIt->first) {
                        auto syncInfoMapIt = _syncInfoMap.begin();
                        while (syncInfoMapIt != _syncInfoMap.end()) {
                            // Erase syncs linked
                            if (syncInfoMapIt->second.driveDbId() == driveInfoMapIt->first) {
                                syncInfoMapIt = _syncInfoMap.erase(syncInfoMapIt);
                                continue;
                            }
                            syncInfoMapIt++;
                        }
                        driveInfoMapIt = _driveInfoMap.erase(driveInfoMapIt);
                        continue;
                    }
                    driveInfoMapIt++;
                }
                accountInfoMapIt = _accountInfoMap.erase(accountInfoMapIt);
                continue;
            }
            accountInfoMapIt++;
        }

        // Erase user
        userInfoMapIt = _userInfoMap.erase(userInfoMapIt);
        if (_currentUserDbId == userDbId) {
            // Select next user if there is one
            _currentUserDbId = (userInfoMapIt == _userInfoMap.end() ? 0 : userInfoMapIt->first);

            if (_currentUserDbId) {
                auto accountInfoMapIt = _accountInfoMap.begin();
                while (accountInfoMapIt != _accountInfoMap.end()) {
                    if (accountInfoMapIt->second.userDbId() == _currentUserDbId) {
                        _currentAccountDbId = accountInfoMapIt->first;
                        break;
                    }
                    accountInfoMapIt++;
                }
            }

            if (_currentAccountDbId) {
                auto driveInfoMapIt = _driveInfoMap.begin();
                while (driveInfoMapIt != _driveInfoMap.end()) {
                    if (driveInfoMapIt->second.accountDbId() == _currentAccountDbId) {
                        _currentDriveDbId = driveInfoMapIt->first;
                        break;
                    }
                    driveInfoMapIt++;
                }
            }
        }

        emit userListRefreshed();
        emit refreshStatusNeeded();
    }
}

void ClientGui::onAccountAdded(const AccountInfo &accountInfo) {
    _accountInfoMap.insert({accountInfo.dbId(), accountInfo});

    if (!_currentAccountDbId) {
        _currentAccountDbId = accountInfo.dbId();
    }

    emit accountListRefreshed();
    emit refreshStatusNeeded();
}

void ClientGui::onAccountUpdated(const AccountInfo &accountInfo) {
    const auto &accountInfoMapIt = _accountInfoMap.find(accountInfo.dbId());
    if (accountInfoMapIt != _accountInfoMap.end()) {
        accountInfoMapIt->second.setUserDbId(accountInfo.userDbId());

        emit accountListRefreshed();
    }
}

void ClientGui::onAccountRemoved(int accountDbId) {
    auto accountInfoMapIt = _accountInfoMap.find(accountDbId);
    if (accountInfoMapIt != _accountInfoMap.end()) {
        // Erase accounts linked
        auto driveInfoMapIt = _driveInfoMap.begin();
        while (driveInfoMapIt != _driveInfoMap.end()) {
            // Erase drives linked
            if (driveInfoMapIt->second.accountDbId() == accountInfoMapIt->first) {
                auto syncInfoMapIt = _syncInfoMap.begin();
                while (syncInfoMapIt != _syncInfoMap.end()) {
                    // Erase syncs linked
                    if (syncInfoMapIt->second.driveDbId() == driveInfoMapIt->first) {
                        syncInfoMapIt = _syncInfoMap.erase(syncInfoMapIt);
                        continue;
                    }
                    syncInfoMapIt++;
                }
                driveInfoMapIt = _driveInfoMap.erase(driveInfoMapIt);
                continue;
            }
            driveInfoMapIt++;
        }

        // Erase account
        accountInfoMapIt = _accountInfoMap.erase(accountInfoMapIt);
        if (_currentAccountDbId == accountDbId) {
            // Select next account if there is one
            _currentAccountDbId = (accountInfoMapIt == _accountInfoMap.end() ? 0 : accountInfoMapIt->first);

            if (_currentAccountDbId) {
                auto driveInfoMapIt = _driveInfoMap.begin();
                while (driveInfoMapIt != _driveInfoMap.end()) {
                    if (driveInfoMapIt->second.accountDbId() == _currentAccountDbId) {
                        _currentDriveDbId = driveInfoMapIt->first;
                        break;
                    }
                    driveInfoMapIt++;
                }
            }
        }

        emit accountListRefreshed();
        emit refreshStatusNeeded();
    }
}

void ClientGui::onDriveAdded(const DriveInfo &driveInfo) {
    _driveInfoMap.insert({driveInfo.dbId(), DriveInfoClient(driveInfo)});

    if (!_currentDriveDbId) {
        _currentDriveDbId = driveInfo.dbId();
    }

    emit driveListRefreshed();
    emit refreshStatusNeeded();
}

void ClientGui::onDriveUpdated(const DriveInfo &driveInfo) {
    const auto &driveInfoMapIt = _driveInfoMap.find(driveInfo.dbId());
    if (driveInfoMapIt != _driveInfoMap.end()) {
        driveInfoMapIt->second.setAccountDbId(driveInfo.accountDbId());
        driveInfoMapIt->second.setName(driveInfo.name());
        driveInfoMapIt->second.setColor(driveInfo.color());

        emit driveListRefreshed();
    }
}

void ClientGui::onDriveQuotaUpdated(int driveDbId, qint64 total, qint64 used) {
    const auto &driveInfoMapIt = _driveInfoMap.find(driveDbId);
    if (driveInfoMapIt != _driveInfoMap.end()) {
        driveInfoMapIt->second.setTotalSize(total);
        driveInfoMapIt->second.setUsed(used);

        emit driveQuotaUpdated(driveDbId);
    }
}

void ClientGui::onRemoveDrive(int driveDbId) {
    auto driveInfoMapIt = _driveInfoMap.find(driveDbId);
    if (driveInfoMapIt == _driveInfoMap.end()) {
        return;
    }

    CustomMessageBox msgBox(QMessageBox::Question,
                            tr("Do you really want to remove the synchronizations of the account <i>%1</i> ?<br>"
                               "<b>Note:</b> This will <b>not</b> delete any files.")
                                    .arg(driveInfoMapIt->second.name()),
                            QMessageBox::NoButton);
    msgBox.addButton(tr("REMOVE ALL SYNCHRONIZATIONS"), QMessageBox::Yes);
    msgBox.addButton(tr("CANCEL"), QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);

    if (msgBox.exec() == QMessageBox::Yes) {
        emit driveBeingRemoved(); // Lock drive-related GUI actions.
        try {
            ExitCode exitCode = GuiRequests::deleteDrive(driveDbId);
            if (exitCode != ExitCode::Ok) {
                qCWarning(lcClientGui()) << "Error in Requests::deleteDrive for driveDbId=" << driveDbId;
                onDriveDeletionFailed(driveDbId);
            }
        } catch (std::exception const &) {
            onDriveDeletionFailed(driveDbId);
        }
    }
}

void ClientGui::onDriveDeletionFailed(int driveDbId) {
    auto driveInfoMapIt = _driveInfoMap.find(driveDbId);
    assert(driveInfoMapIt != _driveInfoMap.cend());

    // Unlock drive-related GUI actions.
    driveInfoMapIt->second.setIsBeingDeleted(false);

    emit driveListRefreshed();
    emit refreshStatusNeeded();
}

void ClientGui::onDriveRemoved(int driveDbId) {
    if (auto driveInfoMapIt = _driveInfoMap.find(driveDbId); driveInfoMapIt != _driveInfoMap.end()) {
        for (auto syncInfoMapIt = _syncInfoMap.begin(); syncInfoMapIt != _syncInfoMap.end();) {
            // Erase linked synchronizations
            if (syncInfoMapIt->second.driveDbId() == driveDbId) {
                syncInfoMapIt = _syncInfoMap.erase(syncInfoMapIt);
            } else {
                ++syncInfoMapIt;
            }
        }

        // Erase drive
        driveInfoMapIt = _driveInfoMap.erase(driveInfoMapIt);

        // Handle the case of the current drive being erased
        if (_currentDriveDbId == driveDbId) {
            // Select the next drive if there is one.
            if (driveInfoMapIt != _driveInfoMap.cend()) {
                _currentDriveDbId = driveInfoMapIt->first;
            } else if (_driveInfoMap.cbegin() != _driveInfoMap.cend()) {
                _currentDriveDbId = _driveInfoMap.cbegin()->first;
            } else {
                _currentDriveDbId = 0;
            }
        }

        emit driveListRefreshed();
        emit refreshStatusNeeded();
    }
}

void ClientGui::onSyncAdded(const SyncInfo &syncInfo) {
    _syncInfoMap.insert({syncInfo.dbId(), SyncInfoClient(syncInfo)});

    emit syncListRefreshed();
    emit refreshStatusNeeded();
}

void ClientGui::onSyncUpdated(const SyncInfo &syncInfo) {
    const auto &syncInfoMapIt = _syncInfoMap.find(syncInfo.dbId());
    if (syncInfoMapIt != _syncInfoMap.end()) {
        syncInfoMapIt->second.setDriveDbId(syncInfo.driveDbId());
        syncInfoMapIt->second.setLocalPath(syncInfo.localPath());
        syncInfoMapIt->second.setTargetPath(syncInfo.targetPath());
        syncInfoMapIt->second.setTargetNodeId(syncInfo.targetNodeId());
        syncInfoMapIt->second.setVirtualFileMode(syncInfo.virtualFileMode());
        syncInfoMapIt->second.setNavigationPaneClsid(syncInfo.navigationPaneClsid());

        emit syncListRefreshed();
    }
}

void ClientGui::onRemoveSync(int syncDbId) {
    const auto &syncInfoMapIt = _syncInfoMap.find(syncDbId);
    if (syncInfoMapIt != _syncInfoMap.end()) {
        CommonGuiUtility::removeDirIcon(syncInfoMapIt->second.localPath());
    }
    const ExitCode exitCode = GuiRequests::deleteSync(syncDbId);
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcClientGui()) << "Error in Requests::deleteSync for syncDbId=" << syncDbId;
    }
}

void ClientGui::onSyncDeletionFailed(int syncDbId) {
    // Unlock sync GUI actions.
    auto syncInfoMapIt = _syncInfoMap.find(syncDbId);
    assert(syncInfoMapIt != _syncInfoMap.cend());
    syncInfoMapIt->second.setIsBeingDeleted(false);

    emit syncListRefreshed();
    emit refreshStatusNeeded();
}

void ClientGui::onSyncRemoved(int syncDbId) {
    // Erase sync
    _syncInfoMap.erase(syncDbId);

    emit syncListRefreshed();
    emit refreshStatusNeeded();
}

void ClientGui::onProgressInfo(int syncDbId, SyncStatus status, SyncStep step, int64_t currentFile, int64_t totalFiles,
                               int64_t completedSize, int64_t totalSize, int64_t estimatedRemainingTime) {
    const auto &syncInfoMapIt = _syncInfoMap.find(syncDbId);
    if (syncInfoMapIt != _syncInfoMap.end()) {
        syncInfoMapIt->second.setStatus(status);
        syncInfoMapIt->second.setStep(step);
        syncInfoMapIt->second.setCurrentFile(currentFile);
        syncInfoMapIt->second.setTotalFiles(totalFiles);
        syncInfoMapIt->second.setCompletedSize(completedSize);
        syncInfoMapIt->second.setTotalSize(totalSize);
        syncInfoMapIt->second.setEstimatedRemainingTime(estimatedRemainingTime);

        // Compute drive status
        const auto &driveInfoMapIt = _driveInfoMap.find(syncInfoMapIt->second.driveDbId());
        if (driveInfoMapIt != _driveInfoMap.end()) {
            std::map<int, SyncInfoClient> syncInfoMap;
            loadSyncInfoMap(driveInfoMapIt->first, syncInfoMap);
            driveInfoMapIt->second.updateStatus(syncInfoMap);
        }

        emit refreshStatusNeeded();
        emit updateProgress(syncDbId);
        if (status == SyncStatus::Idle || status == SyncStatus::Paused || status == SyncStatus::Stopped) {
            emit syncFinished(syncDbId);
        }
    }
}

void ClientGui::onExecuteSyncAction(ActionType type, ActionTarget target, int dbId) {
    if (target == ActionTarget::Sync) {
        executeSyncAction(type, dbId);
    } else if (target == ActionTarget::Drive) {
        for (const auto &syncInfoMapElt: _syncInfoMap) {
            if (syncInfoMapElt.second.driveDbId() == dbId) {
                executeSyncAction(type, syncInfoMapElt.first);
            }
        }
    } else {
        for (const auto &syncInfoMapElt: _syncInfoMap) {
            executeSyncAction(type, syncInfoMapElt.first);
        }
    }
}

void ClientGui::onRefreshStatusNeeded() {
    // resetSystray();
    computeOverallSyncStatus();
}

void ClientGui::retranslateUi() {
#ifdef Q_OS_LINUX
    if (_actionSynthesis) _actionSynthesis->setText(QString(tr("Synthesis")));
    if (_actionPreferences) _actionPreferences->setText(QString(tr("Preferences")));
    if (_actionQuit) _actionQuit->setText(QString(tr("Quit")));
#endif
}

void ClientGui::activateLoadInfo(bool value) {
    ExitCode exitCode = GuiRequests::activateLoadInfo(value);
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcClientGui()) << "Error in Requests::activateLoadInfo";
        return;
    }
}

void ClientGui::onShowParametersDialog(int syncDbId, bool errorPage) {
    if (_parametersDialog.isNull()) {
        setupParametersDialog();
    }
    if (syncDbId == 0) {
        if (errorPage) {
            _parametersDialog->openGeneralErrorsPage();
        } else {
            _parametersDialog->openPreferencesPage();
        }
    } else {
        if (errorPage) {
            _parametersDialog->openDriveErrorsPage(syncDbId);
        } else {
            _parametersDialog->openDriveParametersPage(syncDbId);
        }
    }
    raiseDialog(_parametersDialog.get());
}

void ClientGui::onShutdown() {
    // explicitly close windows. This is somewhat of a hack to ensure
    // that saving the geometries happens ASAP during a OS shutdown

    // those do delete on close
    if (!_parametersDialog.isNull()) _parametersDialog->close();
}

void ClientGui::raiseDialog(QWidget *raiseWidget) {
    if (auto *activeWindow = QApplication::activeWindow(); activeWindow && activeWindow != raiseWidget) {
        activeWindow->hide();
    }
    if (raiseWidget && !raiseWidget->parentWidget()) {
#if defined(Q_OS_MAC)
        // Open the widget on the current desktop
        WId windowObject = raiseWidget->winId();
        objc_object *nsviewObject = reinterpret_cast<objc_object *>(windowObject);
        objc_object *nsWindowObject = ((id(*)(id, SEL)) objc_msgSend)(nsviewObject, sel_registerName("window"));
        int NSWindowCollectionBehaviorCanJoinAllSpaces = 1 << 0;
        ((id(*)(id, SEL, int)) objc_msgSend)(nsWindowObject, sel_registerName("setCollectionBehavior:"),
                                             NSWindowCollectionBehaviorCanJoinAllSpaces);
#endif

        // Qt has a bug which causes parent-less dialogs to pop-under.
        raiseWidget->showNormal();
        raiseWidget->raise();
        raiseWidget->activateWindow();
    }
}

bool ClientGui::loadInfoMaps() {
    _userInfoMap.clear();
    _accountInfoMap.clear();
    _driveInfoMap.clear();
    _syncInfoMap.clear();

    // Load user list
    ExitCode exitCode;
    QList<UserInfo> userInfoList;
    exitCode = GuiRequests::getUserInfoList(userInfoList);
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcClientGui()) << "Error in Requests::getUserInfoList";
        return false;
    }

    for (const UserInfo &userInfo: userInfoList) {
        _userInfoMap.insert({userInfo.dbId(), UserInfoClient(userInfo)});
    }

    // Load account list
    QList<AccountInfo> accountInfoList;
    exitCode = GuiRequests::getAccountInfoList(accountInfoList);
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcClientGui()) << "Error in Requests::getAccountInfoList";
        return false;
    }

    for (const AccountInfo &accountInfo: accountInfoList) {
        _accountInfoMap.insert({accountInfo.dbId(), accountInfo});
    }

    // Load drive list
    QList<DriveInfo> driveInfoList;
    exitCode = GuiRequests::getDriveInfoList(driveInfoList);
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcClientGui()) << "Error in Requests::getDriveInfoList";
        return false;
    }

    for (const DriveInfo &driveInfo: driveInfoList) {
        _driveInfoMap.insert({driveInfo.dbId(), DriveInfoClient(driveInfo)});
        if (!_currentDriveDbId) {
            setCurrentDriveDbId(driveInfo.dbId());
        }
    }

    // Load sync list
    QList<SyncInfo> syncInfoList;
    exitCode = GuiRequests::getSyncInfoList(syncInfoList);
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcClientGui()) << "Error in Requests::getSyncInfoList";
        return false;
    }

    for (const SyncInfo &syncInfo: syncInfoList) {
        _syncInfoMap.insert({syncInfo.dbId(), SyncInfoClient(syncInfo)});
    }

    return true;
}

void ClientGui::openLoginDialog(int userDbId, bool invalidTokenError) {
    bool accepted = false;
    _loginDialog.reset(new LoginDialog(userDbId));
    _loginDialog->setAttribute(Qt::WA_DeleteOnClose);

    QEventLoop loop;
    loop.connect(_loginDialog.get(), &QDialog::accepted, &loop, [&]() {
        accepted = true;
        loop.quit();
    });
    loop.connect(_loginDialog.get(), &QDialog::rejected, &loop, [&]() { loop.quit(); });
    loop.connect(_loginDialog.get(), &QDialog::destroyed, &loop, [&]() { loop.quit(); });
    _loginDialog->show();
    loop.exec();
    static_cast<void>(_loginDialog.release());

    if (accepted) {
        qCDebug(lcClientGui()) << "Login OK for userDbId=" << userDbId;

        if (invalidTokenError) {
            // Delete invalid token errors
            GuiRequests::deleteInvalidTokenErrors();

            // Refresh server errors counter and lists
            refreshErrorList(0);
        }

        qCDebug(lcClientGui()) << "startSyncs for userDbId=" << userDbId;
        ExitCode exitCode = GuiRequests::startSyncs(userDbId);
        if (exitCode != ExitCode::Ok) {
            qCWarning(lcClientGui()) << "Error in GuiRequests::startSyncs for userDbId=" << userDbId;
            CustomMessageBox msgBox(QMessageBox::Warning, tr("Failed to start synchronizations!"), QMessageBox::Ok);
            msgBox.exec();
        }
    }
}

void ClientGui::restoreGeometry(QWidget *w) {
    QByteArray dialogGeometry = ParametersCache::instance()->parametersInfo().dialogGeometry(w->objectName());
    if (!dialogGeometry.isEmpty()) {
        if (!w->restoreGeometry(dialogGeometry)) {
            qCWarning(lcClientGui()) << "Error in QWidget::restoreGeometry for objectName=" << w->objectName();
        }
    }
}

} // namespace KDC
