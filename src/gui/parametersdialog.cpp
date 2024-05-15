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

#include "clientgui.h"
#include "gui/errortabwidget.h"
#include "parametersdialog.h"
#include "bottomwidget.h"
#include "actionwidget.h"
#include "custommessagebox.h"
#include "debugreporter.h"
#include "guiutility.h"
#include "config.h"
#include "enablestateholder.h"
#include "languagechangefilter.h"
#include "genericerroritemwidget.h"
#include "guirequests.h"
#include "parameterscache.h"
#include "libcommongui/logger.h"
#include "libcommon/asserts.h"
#include "libcommon/utility/utility.h"
#include "libcommongui/utility/utility.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QGraphicsDropShadowEffect>
#include <QLabel>
#include <QLoggingCategory>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QUrl>
#include <QVBoxLayout>

#define LOGFILE_SERVER_EXT "*_%1.log"
#define LOGFILE_CLIENT_EXT "*_%1_client.log.0"
#define ZLOGFILE_SERVER_EXT "*_%1.log.*.gz"
#define ZLOGFILE_CLIENT_EXT "*_%1_client.log.*.gz"

#define MANUALTRANSFER_URL "https://www.swisstransfer.com/%1"

namespace KDC {

static const int boxHMargin = 20;
static const int boxVTMargin = 15;
static const int boxVBMargin = 5;
static const int boxVSpacing = 20;
static const int defaultPageSpacing = 20;
static const int defaultLogoIconSize = 50;
static const int maxLogFilesToSend = 25;

Q_LOGGING_CATEGORY(lcParametersDialog, "gui.parametersdialog", QtInfoMsg)

ParametersDialog::ParametersDialog(std::shared_ptr<ClientGui> gui, QWidget *parent) : CustomDialog(false, parent), _gui(gui) {
    initUI();

    connect(this, &ParametersDialog::exit, this, &ParametersDialog::onExit);
}

void ParametersDialog::openPreferencesPage() {
    onDisplayPreferences();
    forceRedraw();
}

void ParametersDialog::openGeneralErrorsPage() {
    openPreferencesPage();
    onDisplayGeneralErrors();
    forceRedraw();
}

void ParametersDialog::openDriveErrorsPage(int driveDbId) {
    openDriveParametersPage(driveDbId);
    onDisplayDriveErrors(driveDbId);
    forceRedraw();
}

void ParametersDialog::resolveConflictErrors(int driveDbId, bool keepLocalVersion) {
    _gui->resolveConflictErrors(driveDbId, keepLocalVersion);
}

void ParametersDialog::resolveUnsupportedCharErrors(int driveDbId) {
    _gui->resolveUnsupportedCharErrors(driveDbId);
}

void ParametersDialog::openDriveParametersPage(int driveDbId) {
    onDisplayDriveParameters();
    _driveMenuBarWidget->driveSelectionWidget()->selectDrive(driveDbId);
    forceRedraw();
}

void ParametersDialog::initUI() {
    /*
     *  _pageStackedWidget
     *      drivePageWidget
     *          driveBox
     *              _driveMenuBarWidget
     *              _drivePreferencesScrollArea
     *                  _drivePreferencesWidget
     *      preferencesPageWidget
     *          preferencesVBox
     *              _preferencesMenuBarWidget
     *              preferencesScrollArea
     *                  _preferencesWidget
     *      errorsPageWidget
     *          errorsVBox
     *              _errorsMenuBarWidget
     *              errorsHeaderVBox
     *                  sendLogsWidget
     *                  historyLabel
     *                  clearButton
     *              _errorsStackedWidget
     *                  ErrorTabWidget
     *                      unresolvederrorsListWidget[]
     *                      resolvederrorsListWidget[]
     *  bottomWidget
     */

    setObjectName("ParametersDialog");
    ClientGui::restoreGeometry(this);
    setResizable(true);

    // Page stacked widget
    _pageStackedWidget = new QStackedWidget(this);
    _pageStackedWidget->setMouseTracking(true);
    mainLayout()->addWidget(_pageStackedWidget);

    // Bottom
    BottomWidget *bottomWidget = new BottomWidget(this);
    mainLayout()->addWidget(bottomWidget);

    //
    // Drive page widget
    //
    QWidget *drivePageWidget = new QWidget(this);
    drivePageWidget->setContentsMargins(0, 0, 0, 0);
    _pageStackedWidget->insertWidget(Page::Drive, drivePageWidget);

    QVBoxLayout *driveVBox = new QVBoxLayout();
    driveVBox->setContentsMargins(0, 0, 0, 0);
    driveVBox->setSpacing(0);
    drivePageWidget->setLayout(driveVBox);

    // Drive menu bar
    _driveMenuBarWidget = new MainMenuBarWidget(_gui, this);
    driveVBox->addWidget(_driveMenuBarWidget);

    // Drive preferences
    _drivePreferencesWidget = new DrivePreferencesWidget(_gui, this);

    _drivePreferencesScrollArea = new QScrollArea(this);
    _drivePreferencesScrollArea->setWidget(_drivePreferencesWidget);
    _drivePreferencesScrollArea->setWidgetResizable(true);
    _drivePreferencesScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _drivePreferencesScrollArea->setVisible(false);
    driveVBox->addWidget(_drivePreferencesScrollArea);
    driveVBox->setStretchFactor(_drivePreferencesScrollArea, 1);

    // No drive page
    _noDrivePagewidget = new QWidget(this);
    _noDrivePagewidget->setObjectName("noDrivePagewidget");
    driveVBox->addWidget(_noDrivePagewidget);
    driveVBox->setStretchFactor(_noDrivePagewidget, 1);

    QVBoxLayout *vboxLayout = new QVBoxLayout();
    vboxLayout->setSpacing(defaultPageSpacing);
    _noDrivePagewidget->setLayout(vboxLayout);

    vboxLayout->addStretch();

    QLabel *iconLabel = new QLabel(this);
    iconLabel->setAlignment(Qt::AlignHCenter);
    iconLabel->setPixmap(QIcon(":/client/resources/icons/document types/file-default.svg")
                             .pixmap(QSize(defaultLogoIconSize, defaultLogoIconSize)));
    vboxLayout->addWidget(iconLabel);

    _defaultTextLabel = new QLabel(this);
    _defaultTextLabel->setObjectName("defaultTextLabel");
    _defaultTextLabel->setAlignment(Qt::AlignHCenter);
    _defaultTextLabel->setWordWrap(true);
    vboxLayout->addWidget(_defaultTextLabel);
    vboxLayout->addStretch();

    //
    // Preferences page widget
    //
    QWidget *preferencesPageWidget = new QWidget(this);
    preferencesPageWidget->setContentsMargins(0, 0, 0, 0);
    _pageStackedWidget->insertWidget(Page::Preferences, preferencesPageWidget);

    QVBoxLayout *preferencesVBox = new QVBoxLayout();
    preferencesVBox->setContentsMargins(0, 0, 0, 0);
    preferencesVBox->setSpacing(0);
    preferencesPageWidget->setLayout(preferencesVBox);

    // Preferences menu bar
    _preferencesMenuBarWidget = new PreferencesMenuBarWidget(this);
    preferencesVBox->addWidget(_preferencesMenuBarWidget);

    // Preferences
    _preferencesWidget = new PreferencesWidget(_gui, this);

    QScrollArea *preferencesScrollArea = new QScrollArea(this);
    preferencesScrollArea->setWidget(_preferencesWidget);
    preferencesScrollArea->setWidgetResizable(true);
    preferencesScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    preferencesVBox->addWidget(preferencesScrollArea);
    preferencesVBox->setStretchFactor(preferencesScrollArea, 1);

    //
    // Errors page widget
    //
    QWidget *errorsPageWidget = new QWidget(this);
    errorsPageWidget->setContentsMargins(0, 0, 0, 0);
    _pageStackedWidget->insertWidget(Page::Errors, errorsPageWidget);

    QVBoxLayout *errorsVBox = new QVBoxLayout();
    errorsVBox->setContentsMargins(0, 0, 0, 0);
    errorsVBox->setSpacing(0);
    errorsPageWidget->setLayout(errorsVBox);

    // Error menu bar
    _errorsMenuBarWidget = new ErrorsMenuBarWidget(_gui, this);
    errorsVBox->addWidget(_errorsMenuBarWidget);

    // Errors header
    QWidget *errorsHeaderWidget = new QWidget(this);
    errorsHeaderWidget->setContentsMargins(0, 0, 0, 0);
    errorsHeaderWidget->setObjectName("errorsHeaderWidget");
    errorsVBox->addWidget(errorsHeaderWidget);

    QVBoxLayout *errorsHeaderVBox = new QVBoxLayout();
    errorsHeaderVBox->setContentsMargins(boxHMargin, boxVTMargin, boxHMargin, boxVBMargin);
    errorsHeaderVBox->setSpacing(boxVSpacing);
    errorsHeaderWidget->setLayout(errorsHeaderVBox);

    _sendLogsWidget = new ActionWidget(":/client/resources/icons/actions/help.svg", this);
    _sendLogsWidget->setObjectName("sendLogsWidget");
    errorsHeaderVBox->addWidget(_sendLogsWidget);

    // Errors stacked widget
    _errorsStackedWidget = new QStackedWidget(this);
    _errorsStackedWidget->setObjectName("errorsStackedWidget");

    errorsVBox->addWidget(_errorsStackedWidget);
    errorsVBox->setStretchFactor(_errorsStackedWidget, 1);

    // Create General level errors list
    _errorTabWidgetStackPosition =
        _errorsStackedWidget->addWidget(new ErrorTabWidget(DriveInfoClient::ParametersStackedWidgetGeneral, true, this));
    refreshErrorList(0);

    // Init labels and setup connection for on the fly translation
    retranslateUi();
    LanguageChangeFilter *languageFilter = new LanguageChangeFilter(this);
    installEventFilter(languageFilter);
    connect(languageFilter, &LanguageChangeFilter::retranslate, this, &ParametersDialog::retranslateUi);

    connect(_driveMenuBarWidget, &MainMenuBarWidget::preferencesButtonClicked, this,
            &ParametersDialog::onPreferencesButtonClicked);
    connect(_driveMenuBarWidget, &MainMenuBarWidget::openHelp, this, &ParametersDialog::onOpenHelp);
    connect(_driveMenuBarWidget, &MainMenuBarWidget::driveSelected, this, &ParametersDialog::onDriveSelected);
    connect(_driveMenuBarWidget, &MainMenuBarWidget::addDrive, this, &ParametersDialog::onAddDrive);
    connect(_drivePreferencesWidget, &DrivePreferencesWidget::displayErrors, this, &ParametersDialog::onDisplayDriveErrors);
    connect(_drivePreferencesWidget, &DrivePreferencesWidget::openFolder, this, &ParametersDialog::onOpenFolder);
    connect(_drivePreferencesWidget, &DrivePreferencesWidget::removeDrive, this, &ParametersDialog::onRemoveDrive);
    connect(_drivePreferencesWidget, &DrivePreferencesWidget::runSync, this, &ParametersDialog::onRunSync);
    connect(_drivePreferencesWidget, &DrivePreferencesWidget::pauseSync, this, &ParametersDialog::onPauseSync);
    connect(_drivePreferencesWidget, &DrivePreferencesWidget::resumeSync, this, &ParametersDialog::onResumeSync);
    connect(_preferencesMenuBarWidget, &PreferencesMenuBarWidget::backButtonClicked, this,
            &ParametersDialog::onBackButtonClicked);
    connect(_errorsMenuBarWidget, &ErrorsMenuBarWidget::backButtonClicked, this, &ParametersDialog::onBackButtonClicked);
    connect(_preferencesWidget, &PreferencesWidget::displayErrors, this, &ParametersDialog::onDisplayGeneralErrors);
    connect(_preferencesWidget, &PreferencesWidget::setStyle, this, &ParametersDialog::onSetStyle);
    connect(_preferencesWidget, &PreferencesWidget::undecidedListsCleared, _drivePreferencesWidget,
            &DrivePreferencesWidget::undecidedListsCleared);
    connect(_sendLogsWidget, &ActionWidget::clicked, this, &ParametersDialog::onSendLogs);
    connect(this, &ParametersDialog::clearErrors, this, &ParametersDialog::onClearErrors);
    connect(this, &ParametersDialog::newBigFolder, _drivePreferencesWidget, &DrivePreferencesWidget::newBigFolderDiscovered);
}

void ParametersDialog::createErrorTabWidgetIfNeeded(int driveDbId) {
    const auto &driveInfoMapIt = _gui->driveInfoMap().find(driveDbId);
    if (driveInfoMapIt == _gui->driveInfoMap().end()) {
        qCDebug(lcParametersDialog()) << "Drive not found in drive map for driveDbId=" << driveDbId;
        return;
    }

    if (driveInfoMapIt->second.errorTabWidgetStackPosition() == 0) {
        driveInfoMapIt->second.setErrorTabWidgetStackPosition(
            _errorsStackedWidget->addWidget(new ErrorTabWidget(driveDbId, false, this)));
    }
}

QByteArray ParametersDialog::contents(const QString &path) {
    QFile file(path);
    if (file.open(QFile::ReadOnly)) {
        return file.readAll();
    } else {
        return QByteArray();
    }
}

void ParametersDialog::reset() {
    _currentDriveDbId = 0;
    _driveMenuBarWidget->driveSelectionWidget()->clear();
    _driveMenuBarWidget->progressBarWidget()->reset();
    _drivePreferencesWidget->reset();

    // Clear errorsStackedWidget
    int index = DriveInfoClient::ParametersStackedWidgetFirstAdded;
    while (index < _errorsStackedWidget->count()) {
        _errorsStackedWidget->removeWidget(_errorsStackedWidget->widget(index));
        index++;
    }

    _drivePreferencesScrollArea->setVisible(false);
    _noDrivePagewidget->setVisible(true);
}

QString ParametersDialog::getAppErrorText(QString fctCode, ExitCode exitCode, ExitCause exitCause) const noexcept {
    const QString err = QString("%1:%2:%3").arg(fctCode).arg(exitCode).arg(exitCause);
    // TODO: USELESS CODE : this switch should be simplified !!!!
    switch (exitCode) {
        case ExitCodeUnknown:
            return tr("A technical error has occurred (error %1).<br>"
                      "Please empty the history and if the error persists, contact our support team.")
                .arg(err);
            break;
        case ExitCodeNetworkError:
            if (exitCause == ExitCauseNetworkTimeout) {
                return tr("It seems that your network connection is configured with too low a timeout for the application to "
                          "work correctly (error %1).<br>"
                          "Please check your network configuration.")
                    .arg(err);
            } else {
                return tr("Cannot connect to kDrive server (error %1).<br>"
                          "Attempting reconnection. Please check your Internet connection and your firewall.")
                    .arg(err);
            }
            break;
        case ExitCodeInvalidToken:
            return tr("A login problem has occurred (error %1).<br>"
                      "Please log in again and if the error persists, contact our support team.")
                .arg(err);
            break;
        case ExitCodeDataError:
            return tr("A technical error has occurred (error %1).<br>"
                      "Please empty the history and if the error persists, contact our support team.")
                .arg(err);
            break;
        case ExitCodeDbError:
            if (exitCause == ExitCauseDbAccessError) {
                return tr("A technical error has occurred (error %1).<br>"
                          "Please empty the history and if the error persists, contact our support team.")
                    .arg(err);
            } else {
                return tr("A technical error has occurred (error %1).<br>"
                          "Please empty the history and if the error persists, contact our support team.")
                    .arg(err);
            }
            break;
        case ExitCodeBackError:
            return tr("A technical error has occurred (error %1).<br>"
                      "Please empty the history and if the error persists, contact our support team.")
                .arg(err);
            break;
        case ExitCodeSystemError:
            if (exitCause == ExitCauseMigrationError) {
                return tr(
                    "Old synchronisation database doesn't exist or is not accessible.<br>"
                    "Old blacklist data haven't been migrated.");
            } else {
                return tr("A technical error has occurred (error %1).<br>"
                          "Please empty the history and if the error persists, contact our support team.")
                    .arg(err);
            }
            break;
        case ExitCodeFatalError:
            return tr("A technical error has occurred (error %1).<br>"
                      "Please empty the history and if the error persists, contact our support team.")
                .arg(err);
            break;
        case ExitCodeOk:
        case ExitCodeNeedRestart:
        case ExitCodeInconsistencyError:
        case ExitCodeNoWritePermission:
        case ExitCodeTokenRefreshed:
        case ExitCodeRateLimited:
        case ExitCodeInvalidSync:
            break;
    }

    qCDebug(lcParametersDialog()) << "Unmanaged exit code: " << exitCode;

    return {};
}

QString ParametersDialog::getSyncPalSystemErrorText(const QString &err, ExitCause exitCause) const noexcept {
    switch (exitCause) {
        case ExitCauseNoSearchPermission:
            return tr("The item misses search permission (error %1).<br>"
                      "Please check that you have search/exec access to the parent folder.")
                .arg(err);
        case ExitCauseSyncDirDoesntExist:
            return tr("The synchronization folder is no longer accessible (error %1).<br>"
                      "Synchronization will resume as soon as the folder is accessible.")
                .arg(err);

        case ExitCauseSyncDirReadError:
            return tr("The synchronization folder is inaccessible (error %1).<br>"
                      "Please check that you have read access to this folder.")
                .arg(err);

        case ExitCauseSyncDirWriteError:
            return tr("The synchronization folder is inaccessible (error %1).<br>"
                      "Please check that you have write access to this folder.")
                .arg(err);

        case ExitCauseNotEnoughDiskSpace:
            return tr(
                "There is not enough space left on your disk.<br>"
                "The synchronization has been stopped.");

        case ExitCauseNotEnoughtMemory:
            return tr(
                "There is not enough memory left on your machine.<br>"
                "The synchronization has been stopped.");
        case ExitCauseLiteSyncNotAllowed:
            return tr("Unable to start synchronization (error %1).<br>"
                      "You must allow:<br>"
                      "- kDrive in System Settings >> Privacy & Security >> Security<br>"
                      "- kDrive LiteSync Extension in System Settings >> Privacy & Security >> Full Disk Access.")
                .arg(err);
        case ExitCauseUnableToCreateVfs: {
            if (OldUtility::isWindows()) {
                return tr("Unable to start Lite Sync plugin (error %1).<br>"
                          "Check that the Lite Sync extension is installed and Windows Search service is enabled.<br>"
                          "Please empty the history, restart and if the error persists, contact our support team.")
                    .arg(err);
            } else if (OldUtility::isMac()) {
                return tr("Unable to start Lite Sync plugin (error %1).<br>"
                          "Check that the Lite Sync extension has the correct permissions and is running.<br>"
                          "Please empty the history, restart and if the error persists, contact our support team.")
                    .arg(err);
            } else {
                return tr("Unable to start Lite Sync plugin (error %1).<br>"
                          "Please empty the history, restart and if the error persists, contact our support team.")
                    .arg(err);
            }
        }
        default:
            return tr("A technical error has occurred (error %1).<br>"
                      "Please empty the history and if the error persists, contact our support team.")
                .arg(err);
    }
}

QString ParametersDialog::getSyncPalBackErrorText(const QString &err, ExitCause exitCause, bool userIsAdmin) const noexcept {
    switch (exitCause) {
        case ExitCauseDriveMaintenance:
            return tr(
                "The kDrive is in maintenance mode.<br>"
                "Synchronization will begin again as soon as possible. Please contact our support team if the error "
                "persists.");
        case ExitCauseDriveNotRenew: {
            if (userIsAdmin) {
                return tr(
                    "The kDrive is blocked.<br>"
                    "Please renew kDrive. If no action is taken, the data will be permanently deleted and it will be "
                    "impossible to recover them.");
            } else {
                return tr(
                    "The kDrive is blocked.<br>"
                    "Please contact an administrator to renew the kDrive. If no action is taken, the data will be "
                    "permanently deleted and it will be impossible to recover them.");
            }
        }
        case ExitCauseDriveAccessError:
            return tr(
                "You are not authorised to access this kDrive.<br>"
                "Synchronization has been paused. Please contact an administrator.");
        default:
            return tr("A technical error has occurred (error %1).<br>"
                      "Synchronization will resume as soon as possible. Please contact our support team if the error "
                      "persists.")
                .arg(err);
    }
}

QString ParametersDialog::getSyncPalErrorText(QString fctCode, ExitCode exitCode, ExitCause exitCause,
                                              bool userIsAdmin) const noexcept {
    const QString err = QString("%1:%2:%3").arg(fctCode).arg(exitCode).arg(exitCause);

    switch (exitCode) {
        case ExitCodeUnknown:
            return tr("A technical error has occurred (error %1).<br>"
                      "Please empty the history and if the error persists, contact our support team.")
                .arg(err);
            break;
        case ExitCodeNetworkError:
            if (exitCause == ExitCauseSocketsDefuncted) {
                return tr("The network connections have been dropped by the kernel (error %1).<br>"
                          "Please empty the history and if the error persists, contact our support team.")
                    .arg(err);
            } else {
                return tr("Cannot connect to kDrive server (error %1).<br>"
                          "Attempting reconnection. Please check your Internet connection and your firewall.")
                    .arg(err);
            }
            break;
        case ExitCodeDataError:
            if (exitCause == ExitCauseMigrationError) {
                return tr(
                    "Unfortunately your old configuration could not be migrated.<br>"
                    "The application will use a blank configuration.");
            } else if (exitCause == ExitCauseMigrationProxyNotImplemented) {
                return tr(
                    "Unfortunately your old proxy configuration could not be migrated, SOCKS5 proxies are not supported at this "
                    "time.<br>"
                    "The application will use system proxy settings instead.");
            } else {
                return tr("A technical error has occurred (error %1).<br>"
                          "Synchronization has been restarted. Please empty the history and if the error persists, please "
                          "contact our support team.")
                    .arg(err);
            }
            break;
        case ExitCodeDbError:
            if (exitCause == ExitCauseDbAccessError) {
                return tr("A technical error has occurred (error %1).<br>"
                          "Please empty the history and if the error persists, contact our support team.")
                    .arg(err);
            } else {
                return tr("An error accessing the synchronization database has happened (error %1).<br>"
                          "Synchronization has been stopped.")
                    .arg(err);
            }
            break;
        case ExitCodeBackError:
            return getSyncPalBackErrorText(err, exitCause, userIsAdmin);
        case ExitCodeSystemError:
            return getSyncPalSystemErrorText(err, exitCause);
        case ExitCodeFatalError:
            return tr("A technical error has occurred (error %1).<br>"
                      "Please empty the history and if the error persists, contact our support team.")
                .arg(err);
            break;
        case ExitCodeInvalidToken:
            return tr("A login problem has occurred (error %1).<br>"
                      "Token invalid or revoked.")
                .arg(err);
            break;
        case ExitCodeInvalidSync:
            return tr("Nested synchronizations are prohibited (error %1).<br>"
                      "You should only keep synchronizations whose folders are not nested.")
                .arg(err);
        case ExitCodeNoWritePermission:
            return tr(
                "The app does not have write rights to the synchronization folder.<br>"
                "The synchronization has been stopped.");
            break;
        case ExitCodeOk:
        case ExitCodeNeedRestart:
        case ExitCodeInconsistencyError:
        case ExitCodeTokenRefreshed:
        case ExitCodeRateLimited:
            break;
    }

    qCDebug(lcParametersDialog()) << "Unmanaged exit code: " << exitCode;

    return {};
}

QString ParametersDialog::getConflictText(ConflictType conflictType, ConflictTypeResolution resolution) const noexcept {
    switch (conflictType) {
        case ConflictTypeNone:
            break;
        case ConflictTypeMoveParentDelete:
            return tr(
                "An element was moved to a deleted folder.<br>"
                "The move has been canceled.");
            break;
        case ConflictTypeMoveDelete:
            return tr(
                "This element was moved by another user.<br>"
                "The deletion has been canceled.");
            break;
        case ConflictTypeCreateParentDelete:
            return tr(
                "An element was created in this folder while it was being deleted.<br>"
                "The delete operation has been propagated anyway.");
            break;
        case ConflictTypeMoveMoveSource:
            return tr(
                "This element has been moved somewhere else.<br>"
                "The local operation has been canceled.");
            break;
        case ConflictTypeMoveMoveDest:
            return tr(
                "An element with the same name already exists in this location.<br>"
                "The local element has been renamed.");
            break;
        case ConflictTypeMoveCreate:
            return tr(
                "An element with the same name already exists in this location.<br>"
                "The local operation has been canceled.");
            break;
        case ConflictTypeEditDelete:
            if (resolution == ConflictTypeResolutionDeleteCanceled) {
                return tr(
                    "The content of the file was modified while it was being deleted.<br>"
                    "The deletion has been canceled.");
            } else if (resolution == ConflictTypeResolutionFileMovedToRoot) {
                return tr(
                    "The content of a synchronized element was modified while a parent folder was being deleted (e.g. the folder "
                    "containing the current folder).<br>"
                    "The file has been moved to the root of your kDrive.");
            } else {
                // Should not happen
                return tr(
                    "The content of an already synchronized file has been modified while this one or one of its parent folders "
                    "has been deleted.<br>");
            }
            break;
        case ConflictTypeCreateCreate:
            return tr(
                "An element with the same name already exists in this location.<br>"
                "The local element has been renamed.");
            break;
        case ConflictTypeEditEdit:
            return tr(
                "The file was modified at the same time by another user.<br>"
                "Your modifications have been saved in a copy.");
            break;
        case ConflictTypeMoveMoveCycle:
            return tr(
                "Another user has moved a parent folder of the destination.<br>"
                "The local operation has been canceled.");
            break;
    }

    qCDebug(lcParametersDialog()) << "Unmanaged conflict type: " << conflictType;

    return {};
}

QString ParametersDialog::getInconsistencyText(InconsistencyType inconsistencyType) const noexcept {
    const auto inconsistencyTypeInt = static_cast<int>(inconsistencyType);
    QString text;

    if (inconsistencyTypeInt & InconsistencyTypeCase) {
        text += tr(
            "An existing file/directory has an identical name with the same case options (same upper and lower case letters).<br>"
            "The file/directory has been temporarily blacklisted.");
    }
    if (inconsistencyTypeInt & InconsistencyTypeForbiddenChar) {
        text += (text.isEmpty() ? "" : "\n");
        text +=
            tr("The file/directory name contains an unsupported character.<br>"
               "The file/directory has been temporarily blacklisted.");
    }
    if (inconsistencyTypeInt & InconsistencyTypeReservedName) {
        text += (text.isEmpty() ? "" : "\n");
        text +=
            tr("This file/directory name is reserved by your operating system.<br>"
               "The file/directory has been temporarily blacklisted.");
    }
    if (inconsistencyTypeInt & InconsistencyTypeNameLength) {
        text += (text.isEmpty() ? "" : "\n");
        text +=
            tr("The file/directory name is too long.<br>"
               "The file/directory has been temporarily blacklisted.");
    }
    if (inconsistencyTypeInt & InconsistencyTypePathLength) {
        text += (text.isEmpty() ? "" : "\n");
        text +=
            tr("The file/directory path is too long.<br>"
               "The file/directory is ignored.");
    }
    if (inconsistencyTypeInt & InconsistencyTypeNotYetSupportedChar) {
        text += (text.isEmpty() ? "" : "\n");
        text +=
            tr("The file/directory name contains a recent UNICODE character not yet supported by your filesystem.<br>"
               "The parent directory has been excluded from synchronization.");
    }
    if (inconsistencyTypeInt & InconsistencyTypeDuplicateNames) {
        text += (text.isEmpty() ? "" : "\n");
        text +=
            tr("The file/directory name coincides with the name of another item in the same directory.<br>"
               "This item is temporarily blacklisted. Consider removing duplicate items.");
    }

    return text;
}

QString ParametersDialog::getCancelText(CancelType cancelType, const QString &path,
                                        const QString &destinationPath /*= ""*/) const noexcept {
    switch (cancelType) {
        case CancelTypeCreate: {
            return tr(
                "You are not allowed to create item.<br>"
                "The item has been excluded from synchronization.");
        }
        case CancelTypeEdit: {
            return tr(
                "You are not allowed to edit item.<br>"
                "The file containing your modifications has been renamed and excluded from synchronization.");
        }
        case CancelTypeMove: {
            QFileInfo fileInfo(path);
            QFileInfo destFileInfo(destinationPath);
            if (fileInfo.dir() == destFileInfo.dir()) {
                // Rename
                return tr(
                    "You are not allowed to rename item.<br>"
                    "It will be restored with its original name.");
            } else {
                // Move
                return tr("You are not allowed to move item to \"%1\".<br>"
                          "It will be restored to its original location.")
                    .arg(destinationPath);
            }
        }
        case CancelTypeDelete: {
            return tr(
                "You are not allowed to delete item.<br>"
                "It will be restored to its original location.");
        }
        case CancelTypeAlreadyExistRemote: {
            return tr("\"%1\" already exists on remote kDrive. It is not synced because it has been blacklisted.").arg(path);
        }
        case CancelTypeMoveToBinFailed: {
            return tr("Failed to move item \"%1\" to bin, it has been blacklisted.").arg(path);
        }
        case CancelTypeAlreadyExistLocal: {
            return tr("\"%1\" already exists on local file system. It is not synced.").arg(path);
        }
        case CancelTypeTmpBlacklisted: {
            return tr("Failed to synchronize item \"%1\". It has been temporarily blacklisted.<br>"
                      "Another attempt to sync it will be done in one hour or on next application startup.")
                .arg(path);
        }
        case CancelTypeExcludedByTemplate: {
            return tr("The item \"%1\" has been excluded from sync by a custom template.<br>"
                      "You can disable this type of notification from the Preferences")
                .arg(path);
        }
        case CancelTypeHardlink: {
            return tr("The item \"%1\" has been excluded from sync because it's an hard link").arg(path);
        }
        default: {
            break;
        }
    }

    qCDebug(lcParametersDialog()) << "Unmanaged cancel type: " << cancelType;

    return {};
}

QString ParametersDialog::getBackErrorText(const ErrorInfo &errorInfo) const noexcept {
    switch (errorInfo.exitCause()) {
        case ExitCauseHttpErrForbidden: {
            return tr("The operation performed on item %1 is forbidden.<br>"
                      "The file/directory has been temporarily blacklisted.")
                .arg(errorInfo.path());
        }
        case ExitCauseApiErr:
        case ExitCauseUploadNotTerminated: {
            return tr("The operation performed on item %1 failed.<br>"
                      "The file/directory has been temporarily blacklisted.")
                .arg(errorInfo.path());
        }
        case ExitCauseFileTooBig: {
            return tr("The file \"%1\" is too large to be uploaded. It has been temporarily blacklisted.").arg(errorInfo.path());
        }
        case ExitCauseQuotaExceeded: {
            return tr("You have exceeded your quota. Increase your space quota to re-enable file upload.");
        }
        case ExitCauseNotFound: {
            return tr("Impossible to download file \"%1\"").arg(errorInfo.path());
        }
        default:
            return tr("Synchronization error.");
    }
}

QString ParametersDialog::getErrorLevelNodeText(const ErrorInfo &errorInfo) const noexcept {
    if (errorInfo.conflictType() != ConflictTypeNone) {
        return getConflictText(errorInfo.conflictType(), ConflictTypeResolutionNone);
    }

    if (errorInfo.inconsistencyType() != InconsistencyTypeNone) {
        return getInconsistencyText(errorInfo.inconsistencyType());
    }

    if (errorInfo.cancelType() != CancelTypeNone) {
        return getCancelText(errorInfo.cancelType(), errorInfo.path(), errorInfo.destinationPath());
    }

    switch (errorInfo.exitCode()) {
        case ExitCodeSystemError: {
            if (errorInfo.exitCause() == ExitCauseFileAccessError) {
                return tr("Can't access item %1.<br>"
                          "Please fix the write permissions and restart the synchronization.")
                    .arg(errorInfo.path());
            }

            if (errorInfo.exitCause() == ExitCauseMoveToTrashFailed) {
                return tr("Move to trash failed for item %1").arg(errorInfo.path());
            }
        }
        case ExitCodeBackError: {
            return getBackErrorText(errorInfo);
        }

        default:
            return tr("Synchronization error.");
    }
}

QString ParametersDialog::getErrorMessage(const ErrorInfo &errorInfo) const noexcept {
    switch (errorInfo.level()) {
        case ErrorLevelUnknown: {
            return tr(
                "A technical error has occurred.<br>"
                "Please empty the history and if the error persists, contact our support team.");
            break;
        }
        case ErrorLevelServer: {
            return getAppErrorText(errorInfo.functionName(), errorInfo.exitCode(), errorInfo.exitCause());
            break;
        }
        case ErrorLevelSyncPal: {
            if (const auto &syncInfoMapIt = _gui->syncInfoMap().find(errorInfo.syncDbId());
                syncInfoMapIt != _gui->syncInfoMap().end()) {
                const auto &driveInfoMapIt = _gui->driveInfoMap().find(syncInfoMapIt->second.driveDbId());
                if (driveInfoMapIt == _gui->driveInfoMap().end()) {
                    qCDebug(lcParametersDialog())
                        << "Drive not found in drive map for driveDbId=" << syncInfoMapIt->second.driveDbId();
                    return {};
                }
                return getSyncPalErrorText(errorInfo.workerName(), errorInfo.exitCode(), errorInfo.exitCause(),
                                           driveInfoMapIt->second.admin());
            }
            qCDebug(lcParametersDialog()) << "Sync not found in sync map for syncDbId=" << errorInfo.syncDbId();
            return {};
        }
        case ErrorLevelNode:
            return getErrorLevelNodeText(errorInfo);
    }

    qCDebug(lcParametersDialog()) << "Unmanaged error level : " << errorInfo.level();

    return {};
}

void ParametersDialog::onExit() {
    accept();
}

void ParametersDialog::onConfigRefreshed() {
#ifdef CONSOLE_DEBUG
    std::cout << QTime::currentTime().toString("hh:mm:ss").toStdString() << " - ParametersDialog::onUserListRefreshed"
              << std::endl;
#endif

    if (_gui->driveInfoMap().empty()) {
        reset();
        return;
    }

    // Clear unused Drive level (SyncPal or Node) errors list
    for (int widgetIndex = DriveInfoClient::ParametersStackedWidgetFirstAdded; widgetIndex < _errorsStackedWidget->count();) {
        QWidget *widget = _errorsStackedWidget->widget(widgetIndex);
        bool driveIsFound = false;
        bool driveHasSyncs = false;

        for (const auto &[driveId, driveInfo] : _gui->driveInfoMap()) {
            ErrorTabWidget *errorTabWidget = static_cast<ErrorTabWidget *>(
                _errorsStackedWidget->widget(driveInfo.errorTabWidgetStackPosition()));  // position changed

            if (errorTabWidget == widget) {
                driveIsFound = true;
                std::map<int, SyncInfoClient> syncInfoMap;
                _gui->loadSyncInfoMap(_gui->currentDriveDbId(), syncInfoMap);
                driveHasSyncs = !syncInfoMap.empty();

                ++widgetIndex;
                break;
            }
        }

        if (driveIsFound && driveHasSyncs) continue;

        _errorsStackedWidget->removeWidget(widget);
        delete widget;

        for (auto &[driveId, driveInfo] : _gui->driveInfoMap()) {
            const auto position = driveInfo.errorTabWidgetStackPosition();
            if (position > widgetIndex) driveInfo.setErrorTabWidgetStackPosition(position - 1);
        }
    }

    // Create missing Drive level (SyncPal or Node) errors list
    for (auto &[driveId, driveInfo] : _gui->driveInfoMap()) {
        createErrorTabWidgetIfNeeded(driveInfo.dbId());
        if (driveInfo.errorTabWidgetStackPosition() == 0) {
            driveInfo.setErrorTabWidgetStackPosition(_errorsStackedWidget->addWidget(new ErrorTabWidget(driveId, false)));
        }
        refreshErrorList(driveId);
    }

    if (_currentDriveDbId == 0 || _gui->driveInfoMap().find(_currentDriveDbId) == _gui->driveInfoMap().end()) {
        _currentDriveDbId = _gui->currentDriveDbId();
    }

    _driveMenuBarWidget->driveSelectionWidget()->selectDrive(_currentDriveDbId);
}

void ParametersDialog::onUpdateProgress(int syncDbId) {
    Q_UNUSED(syncDbId)
}

void ParametersDialog::onDriveQuotaUpdated(int driveDbId) {
#ifdef CONSOLE_DEBUG
    std::cout << QTime::currentTime().toString("hh:mm:ss").toStdString()
              << " - ParametersDialog::onQuotaUpdated account: " << driveDbId.toStdString() << std::endl;
#endif

    if (driveDbId == _currentDriveDbId) {
        const auto &driveInfoMapIt = _gui->driveInfoMap().find(driveDbId);
        if (driveInfoMapIt != _gui->driveInfoMap().end()) {
            _driveMenuBarWidget->progressBarWidget()->setUsedSize(driveInfoMapIt->second.totalSize(),
                                                                  driveInfoMapIt->second.used());
        }
    }
}

void ParametersDialog::onRefreshErrorList(int driveDbId) {
    refreshErrorList(driveDbId);
}

void ParametersDialog::onRefreshStatusNeeded() {
    _drivePreferencesWidget->refreshStatus();
}

void ParametersDialog::onItemCompleted(int syncDbId, const SyncFileItemInfo &itemInfo) {
    if (itemInfo.status() != SyncFileStatusError && itemInfo.status() != SyncFileStatusConflict &&
        itemInfo.status() != SyncFileStatusInconsistency) {
        return;
    }

    // Refresh errorlist
    const auto syncInfoMapIt = _gui->syncInfoMap().find(syncDbId);
    if (syncInfoMapIt == _gui->syncInfoMap().end()) {
        qCWarning(lcParametersDialog()) << "Sync not found in sync map for syncDbId=" << syncDbId;
        return;
    }
    refreshErrorList(syncInfoMapIt->second.driveDbId());
}

void ParametersDialog::onPreferencesButtonClicked() {
    _pageStackedWidget->setCurrentIndex(Page::Preferences);
}

void ParametersDialog::onOpenHelp() {
    QDesktopServices::openUrl(QUrl(Theme::instance()->helpUrl()));
}

void ParametersDialog::onDriveSelected(int driveDbId) {
    _currentDriveDbId = driveDbId;

    const auto driveInfoElt = _gui->driveInfoMap().find(driveDbId);
    if (driveInfoElt == _gui->driveInfoMap().end()) {
        qCDebug(lcParametersDialog()) << "Drive not found in drive map for driveDbId=" << driveDbId;
        return;
    }

    _driveMenuBarWidget->progressBarWidget()->setUsedSize(driveInfoElt->second.totalSize(), driveInfoElt->second.used());
    _drivePreferencesWidget->setDrive(driveInfoElt->first, driveInfoElt->second.unresolvedErrorsCount());
    _drivePreferencesScrollArea->setVisible(true);
    _noDrivePagewidget->setVisible(false);
}

void ParametersDialog::onAddDrive() {
    emit addDrive();
}

void ParametersDialog::onRemoveDrive(int driveDbId) {
    EnableStateHolder stateHolder(this);
    emit removeDrive(driveDbId);
}

void ParametersDialog::onDisplayDriveErrors(int driveDbId) {
    const auto driveInfoIt = _gui->driveInfoMap().find(driveDbId);
    if (driveInfoIt == _gui->driveInfoMap().end()) {
        qCDebug(lcParametersDialog()) << "Account id not found in account map!";
        Q_ASSERT(false);
        return;
    }

    _currentDriveDbId = driveDbId;
    _errorsMenuBarWidget->setDrive(driveDbId);
    _pageStackedWidget->setCurrentIndex(Page::Errors);
    _errorsStackedWidget->setCurrentIndex(driveInfoIt->second.errorTabWidgetStackPosition());

    // Refresh errorlist
    refreshErrorList(driveDbId);
}

void ParametersDialog::onDisplayGeneralErrors() {
    _errorsMenuBarWidget->setDrive(0);
    _pageStackedWidget->setCurrentIndex(Page::Errors);
    _errorsStackedWidget->setCurrentIndex(_errorTabWidgetStackPosition);

    // Refresh errorlist
    refreshErrorList(0);
}

void ParametersDialog::onDisplayDriveParameters() {
    _pageStackedWidget->setCurrentIndex(Page::Drive);
}

void ParametersDialog::onDisplayPreferences() {
    _pageStackedWidget->setCurrentIndex(Page::Preferences);
}

void ParametersDialog::onBackButtonClicked() {
    if (_pageStackedWidget->currentIndex() == Page::Preferences) {
        onDisplayDriveParameters();
        return;
    }

    if (_pageStackedWidget->currentIndex() == Page::Errors) {
        if (_errorsStackedWidget->currentIndex() == DriveInfoClient::ParametersStackedWidgetGeneral) {
            onDisplayPreferences();
        } else {
            onDisplayDriveParameters();
        }
    }
}

void ParametersDialog::onSetStyle(bool darkTheme) {
    emit setStyle(darkTheme);
}

void ParametersDialog::onSendLogs() {
    EnableStateHolder _(this);

    if (Theme::instance()->debugReporterUrl().isEmpty()) {
        Q_ASSERT(false);
        return;
    }

    CustomMessageBox msgBox(QMessageBox::Information,
                            tr("Please confirm the transmission of debugging information to our support."), QMessageBox::NoButton,
                            this);
    msgBox.addButton(tr("LAST LOG"), QMessageBox::Yes);
    msgBox.addButton(tr("ALL LOGS"), QMessageBox::YesToAll);
    msgBox.addButton(tr("NO"), QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::Yes);

    const int ret = msgBox.execAndMoveToCenter(this);
    if (ret == QDialog::Rejected || ret == QMessageBox::No) return;

    auto *debugReporter = new DebugReporter(QUrl(Theme::instance()->debugReporterUrl()), this);

    // Write accounts
    int num = 0;
    for (const auto &userInfoMapElt : _gui->userInfoMap()) {
        num++;
        for (const auto &accountInfoMapElt : _gui->accountInfoMap()) {
            if (accountInfoMapElt.second.userDbId() == userInfoMapElt.first) {
                for (const auto &driveInfoMapElt : _gui->driveInfoMap()) {
                    if (driveInfoMapElt.second.accountDbId() == accountInfoMapElt.first) {
                        int userId = 0;
                        if (GuiRequests::getUserIdFromUserDbId(userInfoMapElt.second.dbId(), userId) != ExitCodeOk) {
                            qCWarning(lcParametersDialog()) << "Error in GuiRequests::getUserIdFromUserDbId";
                        }

                        debugReporter->setReportData(DebugReporter::MapKeyType::DriveId, num,
                                                     QString::number(driveInfoMapElt.first).toUtf8());
                        debugReporter->setReportData(DebugReporter::MapKeyType::DriveName, num,
                                                     driveInfoMapElt.second.name().toUtf8());
                        debugReporter->setReportData(DebugReporter::MapKeyType::UserId, num, QString::number(userId).toUtf8());
                        debugReporter->setReportData(DebugReporter::MapKeyType::UserName, num,
                                                     userInfoMapElt.second.name().toUtf8());
                    }
                }
            }
        }
    }

    if (num == 0) {
        qCDebug(lcParametersDialog()) << "No account";
        return;
    }

    // Write logs
    QString temporaryFolderLogDirPath = KDC::Logger::instance()->temporaryFolderLogDirPath();
    QDir dir(temporaryFolderLogDirPath);
    if (dir.exists()) {
        num = 0;

        // Send server & client logs
        for (int i = 0; i < 2; i++) {
            // Include current log
            QStringList files =
                dir.entryList(QStringList(QString(i == 0 ? LOGFILE_SERVER_EXT : LOGFILE_CLIENT_EXT).arg(APPLICATION_NAME)),
                              QDir::Files, QDir::Name | QDir::Reversed);
            QString zlogFile;
            if (!files.empty()) {
                num++;
                QString logFile = files.first();

                zlogFile = logFile + ".gz";
                if (dir.exists(zlogFile)) dir.remove(zlogFile);

                CommonUtility::compressFile(temporaryFolderLogDirPath + dirSeparator + logFile,
                                            temporaryFolderLogDirPath + dirSeparator + zlogFile);

                debugReporter->setReportData(DebugReporter::MapKeyType::LogName, num,
                                             contents(temporaryFolderLogDirPath + dirSeparator + zlogFile),
                                             "application/octet-stream", QFileInfo(zlogFile).fileName().toUtf8());
            }

            if (ret == QMessageBox::YesToAll) {
                // Include archived logs
                QStringList files =
                    dir.entryList(QStringList(QString(i == 0 ? ZLOGFILE_SERVER_EXT : ZLOGFILE_CLIENT_EXT).arg(APPLICATION_NAME)),
                                  QDir::Files, QDir::Name | QDir::Reversed);
                for (const QString &file : files) {
                    if (file.startsWith(zlogFile)) continue;

                    num++;
                    if (num > maxLogFilesToSend) {
                        break;
                    }

                    debugReporter->setReportData(DebugReporter::MapKeyType::LogName, num,
                                                 contents(temporaryFolderLogDirPath + dirSeparator + file),
                                                 "application/octet-stream", QFileInfo(file).fileName().toUtf8());
                }
            }
        }

        if (num == 0) {
            qCDebug(lcParametersDialog()) << "No log file";
            return;
        }
    } else {
        qCDebug(lcParametersDialog()) << "Empty log dir: " << temporaryFolderLogDirPath;
        return;
    }

    connect(debugReporter, &DebugReporter::sent, this, &ParametersDialog::onDebugReporterDone);
    debugReporter->send();
}

void ParametersDialog::onOpenFolder(const QString &filePath) {
    if (!KDC::GuiUtility::openFolder(filePath)) {
        CustomMessageBox msgBox(QMessageBox::Warning, tr("Unable to open folder path %1.").arg(filePath), QMessageBox::Ok, this);
        msgBox.exec();
    }
}

void ParametersDialog::onDebugReporterDone(bool retCode, const QString &debugId) {
    EnableStateHolder _(this);

    Language language = ParametersCache::instance()->parametersInfo().language();
    QString languageCode = KDC::CommonUtility::languageCodeList(language).first();
    QString swistranferUrl = QString(MANUALTRANSFER_URL).arg(languageCode.left(2));

    CustomMessageBox msgBox(QMessageBox::Information,
                            retCode
                                ? tr("Transmission done!<br>Please refer to identifier <b>%1</b> in bug reports.").arg(debugId)
                                : tr("Transmission failed!\nPlease, use the following link to send the logs to the support: <a "
                                     "style=\"%1\" href=\"%2\">%2</a>")
                                      .arg(CommonUtility::linkStyle, swistranferUrl),
                            QMessageBox::Ok, this);

    if (retCode) {
        // Because the debug ID is too long, word wrap is not working
        // Set a static size instead
        msgBox.setMinimumHeight(250);
        msgBox.setMaximumHeight(250);
        msgBox.resize(750, 250);
    }
    msgBox.exec();

    DebugReporter *debugReporter = qobject_cast<DebugReporter *>(sender());
    if (debugReporter) {
        debugReporter->deleteLater();
    }
}

void ParametersDialog::retranslateUi() {
    _defaultTextLabel->setText(tr("No kDrive configured!"));
    _sendLogsWidget->setText(tr("Need help? Generate an application log archive to send to our support team."));
}

void ParametersDialog::onPauseSync(int syncDbId) {
    emit executeSyncAction(ActionTypeStop, ActionTargetSync, syncDbId);
}

void ParametersDialog::onResumeSync(int syncDbId) {
    emit executeSyncAction(ActionTypeStart, ActionTargetSync, syncDbId);
}

void ParametersDialog::onRunSync(int syncDbId) {
    Q_UNUSED(syncDbId)

    // TODO: useless?
}

void ParametersDialog::onClearErrors(int driveDbId, bool autoResolved) {
    ErrorTabWidget *errorTabWidget = nullptr;
    QListWidget *listWidgetToClear = nullptr;

    if (driveDbId == 0) {
        ASSERT(_errorsStackedWidget->currentIndex() == static_cast<int>(DriveInfoClient::ParametersStackedWidgetGeneral));

        errorTabWidget = static_cast<ErrorTabWidget *>(_errorsStackedWidget->widget(_errorTabWidgetStackPosition));

        if (!GuiRequests::deleteErrorsServer()) {
            qCWarning(lcParametersDialog()) << "Error in GuiRequests::deleteErrorsServer";
            return;
        }
    } else {
        const auto driveInfoMapIt = _gui->driveInfoMap().find(driveDbId);
        if (driveInfoMapIt == _gui->driveInfoMap().end()) {
            qCWarning(lcParametersDialog()) << "Drive not found in drive map for driveDbId=" << driveDbId;
            return;
        }

        errorTabWidget =
            static_cast<ErrorTabWidget *>(_errorsStackedWidget->widget(driveInfoMapIt->second.errorTabWidgetStackPosition()));

        for (const auto &syncInfoMapIt : _gui->syncInfoMap()) {
            if (syncInfoMapIt.second.driveDbId() == driveDbId) {
                if (!GuiRequests::deleteErrorsForSync(syncInfoMapIt.first, autoResolved)) {
                    qCWarning(lcParametersDialog()) << "Error in GuiRequests::deleteErrorsForSync syncId=" << syncInfoMapIt.first;
                    return;
                }
            }
        }
    }

    listWidgetToClear =
        autoResolved ? errorTabWidget->autoResolvedErrorsListWidget() : errorTabWidget->unresolvedErrorsListWidget();
    while (listWidgetToClear->count()) {
        QListWidgetItem *listWidgetItem = listWidgetToClear->takeItem(0);
        delete listWidgetItem;
    }

    if (autoResolved) {
        errorTabWidget->setAutoResolvedErrorsCount(0);
    } else {
        errorTabWidget->setUnresolvedErrorsCount(0);
    }
}

void ParametersDialog::refreshErrorList(int driveDbId) {
    ErrorTabWidget *errorTabWidget = nullptr;
    QListWidget *autoresolvedErrorsListWidget = nullptr;
    QListWidget *unresolvedErrorsListWidget = nullptr;
    QList<ErrorInfo> errorInfoList;

    if (driveDbId == 0) {
        // Server level error
        errorTabWidget = static_cast<ErrorTabWidget *>(_errorsStackedWidget->widget(_errorTabWidgetStackPosition));
        autoresolvedErrorsListWidget = errorTabWidget->autoResolvedErrorsListWidget();
        unresolvedErrorsListWidget = errorTabWidget->unresolvedErrorsListWidget();
    } else {
        // Drive level error (SyncPal or Node)
        const auto driveInfoMapIt = _gui->driveInfoMap().find(driveDbId);
        if (driveInfoMapIt == _gui->driveInfoMap().end()) {
            qCWarning(lcParametersDialog()) << "Drive not found in drive map for driveDbId=" << driveDbId;
            return;
        }

        errorTabWidget =
            static_cast<ErrorTabWidget *>(_errorsStackedWidget->widget(driveInfoMapIt->second.errorTabWidgetStackPosition()));
        if (!errorTabWidget) {
            qCWarning(lcParametersDialog()) << "Error tab widget doesn't exist anymore for driveDbId=" << driveDbId;
            return;
        }

        autoresolvedErrorsListWidget = errorTabWidget->autoResolvedErrorsListWidget();
        unresolvedErrorsListWidget = errorTabWidget->unresolvedErrorsListWidget();
    }

    _gui->errorInfoList(driveDbId, errorInfoList);

    auto vb = errorTabWidget->currentIndex() == 0 ? unresolvedErrorsListWidget->verticalScrollBar()
                                                  : autoresolvedErrorsListWidget->verticalScrollBar();
    if (vb->value() > 0) {
        return;
    }

    // Display new errors
    int autoresolvedErrorCount = 0;
    int unresolvedErrorCount = 0;
    autoresolvedErrorsListWidget->clear();
    unresolvedErrorsListWidget->clear();
    errorTabWidget->showResolveConflicts(false);
    errorTabWidget->showResolveUnsupportedCharacters(false);
    for (const auto &errorInfo : errorInfoList) {
        // Find list to update and increase drive error counters
        QListWidget *list = nullptr;
        if (errorInfo.autoResolved()) {
            list = autoresolvedErrorsListWidget;
            autoresolvedErrorCount++;
        } else {
            list = unresolvedErrorsListWidget;
            unresolvedErrorCount++;
        }

        // Get user friendly error message
        const QString errorMsg = getErrorMessage(errorInfo);

        // Insert error item
        QListWidgetItem *listWidgetItem = new QListWidgetItem();
        listWidgetItem->setFlags(Qt::NoItemFlags);
        listWidgetItem->setForeground(Qt::transparent);
        list->insertItem(0, listWidgetItem);

        if (isConflictsWithLocalRename(errorInfo.conflictType())) {
            errorTabWidget->showResolveConflicts(true);
        }
        if (errorInfo.inconsistencyType() == InconsistencyTypeForbiddenChar) {
            errorTabWidget->showResolveUnsupportedCharacters(true);
        }

        GenericErrorItemWidget *widget = nullptr;
        try {
            widget = new GenericErrorItemWidget(_gui, errorMsg, errorInfo, this);
        } catch (std::exception const &) {
            qCWarning(lcParametersDialog()) << "Error in GenericErrorItemWidget::GenericErrorItemWidget for syncDbId="
                                            << errorInfo.syncDbId();
            continue;
        }

        listWidgetItem->setSizeHint(widget->sizeHint());
        list->setItemWidget(listWidgetItem, widget);
    }

    // Update counters
    errorTabWidget->setAutoResolvedErrorsCount(autoresolvedErrorCount);
    errorTabWidget->setUnresolvedErrorsCount(unresolvedErrorCount);
    if (driveDbId == 0) {
        _preferencesWidget->showErrorBanner(unresolvedErrorCount > 0);
    } else {
        _drivePreferencesWidget->showErrorBanner(unresolvedErrorCount > 0);
    }
}

}  // namespace KDC
