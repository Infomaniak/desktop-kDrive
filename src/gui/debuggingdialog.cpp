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

#include "debuggingdialog.h"
#include "clientgui.h"
#include "parameterscache.h"
#include "custommessagebox.h"
#include "enablestateholder.h"
#include "guirequests.h"
#include "libcommon/utility/types.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"
#include "libcommongui/commclient.h"
#include "libcommongui/logger.h"
#include <QDesktopServices>

#include <sstream>
#include <fstream>

#include <map>

#include <QBoxLayout>
#include <QLabel>

namespace KDC {

static const int boxHMargin = 40;
static const int boxHSpacing = 10;
static const int titleBoxVMargin = 25;
static const int recordDebuggingBoxVMargin = 20;
static const int debugLevelLabelBoxVMargin = 10;
static const int debugLevelSelectBoxVMargin = 20;
static const QString debuggingFolderLink = "debuggingFolderLink";
static const QString heavyLogLabelStr = QObject::tr(
    "The entire folder is large (> 100 MB) and may take some time to share. To reduce the sharing time, we recommend that you "
    "share only the last kDrive session.");
Q_LOGGING_CATEGORY(lcDebuggingDialog, "gui.debuggingdialog", QtInfoMsg)

std::map<LogLevel, std::pair<int, QString>> DebuggingDialog::_logLevelMap = {{LogLevel::Debug, {0, QString(tr("Debug"))}},
                                                                             {LogLevel::Info, {1, QString(tr("Info"))}},
                                                                             {LogLevel::Warning, {2, QString(tr("Warning"))}},
                                                                             {LogLevel::Error, {3, QString(tr("Error"))}},
                                                                             {LogLevel::Fatal, {4, QString(tr("Fatal"))}}};

DebuggingDialog::DebuggingDialog(std::shared_ptr<ClientGui> gui, QWidget *parent) : CustomDialog(true, parent), _gui(gui) {
    initUI();
    _recordDebugging = ParametersCache::instance()->parametersInfo().useLog();
    _extendedLog = ParametersCache::instance()->parametersInfo().extendedLog();
    _minLogLevel = ParametersCache::instance()->parametersInfo().logLevel();
    _deleteLogs = ParametersCache::instance()->parametersInfo().purgeOldLogs();

    ClientGui::restoreGeometry(this);
    setResizable(true);

    updateUI();
}

void DebuggingDialog::initUI() {
    setObjectName("DebuggingDialog");

    QVBoxLayout *mainLayout = this->mainLayout();
    // Title
    QLabel *titleLabel = new QLabel();
    titleLabel->setObjectName("titleLabel");
    titleLabel->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    titleLabel->setText(tr("Debugging settings"));
    mainLayout->addWidget(titleLabel);
    mainLayout->addSpacing(titleBoxVMargin);

    // Enable Debug Main Box
    QVBoxLayout *enableDebuggingMainVBox = new QVBoxLayout();
    enableDebuggingMainVBox->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    mainLayout->addLayout(enableDebuggingMainVBox);

    // Enable Debug Main Box | Top Box
    QHBoxLayout *enableDebuggingTopHBox = new QHBoxLayout();
    enableDebuggingMainVBox->addLayout(enableDebuggingTopHBox);

    // Enable Debug Main Box | Top Box | Label (largeNormalBoldTextLabel)
    QLabel *recordDebuggingLabel = new QLabel();
    recordDebuggingLabel->setObjectName("largeNormalBoldTextLabel");
    recordDebuggingLabel->setText(tr("Save debugging information in a folder on my computer (Recommended)"));
    recordDebuggingLabel->setWordWrap(true);
    enableDebuggingTopHBox->setStretch(0, 1);
    enableDebuggingTopHBox->addWidget(recordDebuggingLabel);

    // Enable Debug Main Box | Top Box | Switch
    _recordDebuggingSwitch = new CustomSwitch();
    _recordDebuggingSwitch->setLayoutDirection(Qt::RightToLeft);
    _recordDebuggingSwitch->setAttribute(Qt::WA_MacShowFocusRect, false);
    enableDebuggingTopHBox->addWidget(_recordDebuggingSwitch);

    // Enable Debug Main Box | Label (normalTextLabel)
    QLabel *recordDebuggingInfoLabel = new QLabel();
    recordDebuggingInfoLabel->setObjectName("largeNormalTextLabel");
    recordDebuggingInfoLabel->setText(tr("This information enables IT support to determine the origin of an incident."));
    recordDebuggingInfoLabel->setWordWrap(true);
    enableDebuggingMainVBox->addWidget(recordDebuggingInfoLabel);

    // Debug info main box  (Can be hidden when debugging is disabled)
    _debuggingInfoMainWidget = new QWidget();

    // Debug info main box | Link to debug folder
    _debuggingFolderLabel = new QLabel(_debuggingInfoMainWidget);
    _debuggingFolderLabel->setText(
        tr("<a style=\"%1\" href=\"%2\">Open debugging folder</a>").arg(CommonUtility::linkStyle, debuggingFolderLink));
    _debuggingFolderLabel->setContentsMargins(boxHMargin, 0, 0, 10);
    mainLayout->addWidget(_debuggingFolderLabel);
    mainLayout->addSpacing(boxHSpacing);

    // Scollable main widget
    QWidget *scrollableMainWidget = new QWidget();
    scrollableMainWidget->setObjectName("debugDialogScrollable");

    // Scrollable main widget | Main layout
    QVBoxLayout *scrollableMainLayout = new QVBoxLayout(scrollableMainWidget);

    // Debug info main box  (Can be hidden when debugging is disabled)
    QVBoxLayout *debuggingInfoMainHBox = new QVBoxLayout(_debuggingInfoMainWidget);
    debuggingInfoMainHBox->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    scrollableMainLayout->addWidget(_debuggingInfoMainWidget);

    // Debug info main box | Debug Level Main Box
    QVBoxLayout *debugLevelMainBox = new QVBoxLayout();
    debuggingInfoMainHBox->addLayout(debugLevelMainBox);

    // Debug info main box | Debug Level Main Box | Title box
    QHBoxLayout *debugLevelTitleHBox = new QHBoxLayout();
    debugLevelTitleHBox->setAlignment(Qt::AlignLeft);
    debugLevelMainBox->addLayout(debugLevelTitleHBox);

    // Debug info main box | Debug Level Main Box | Title box | Label (largeNormalBoldTextLabel)
    QLabel *debugLevelTitleLabel = new QLabel();
    debugLevelTitleLabel->setObjectName("largeNormalBoldTextLabel");
    debugLevelTitleLabel->setText(tr("Debug level"));
    debugLevelTitleHBox->addWidget(debugLevelTitleLabel);

    // Debug info main box | Debug Level Main Box | Title box | Helper
    CustomToolButton *debugLevelHelpButton = new CustomToolButton();
    debugLevelHelpButton->setObjectName("helpButton");
    debugLevelHelpButton->setIconPath(":/client/resources/icons/actions/help.svg");
    debugLevelHelpButton->setFixedSize(15, 15);
    debugLevelHelpButton->setEnabled(false);
    debugLevelHelpButton->setToolTip(tr("The trace level lets you choose the extent of the debugging information recorded"));
    debugLevelHelpButton->setToolTipDuration(200000);
    debugLevelTitleHBox->addWidget(debugLevelHelpButton);
    debugLevelMainBox->addSpacing(boxHSpacing / 2);

    // Debug info main box | Debug Level Main Box | Debug level select box
    QHBoxLayout *debugLevelSelectionHBox = new QHBoxLayout();
    debugLevelSelectionHBox->setAlignment(Qt::AlignLeft);
    debugLevelMainBox->addLayout(debugLevelSelectionHBox);

    // Debug info main box | Debug Level Main Box | Debug level select box | combobox
    _debugLevelComboBox = new CustomComboBox();
    _debugLevelComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    _debugLevelComboBox->setAttribute(Qt::WA_MacShowFocusRect, false);
    for (auto const &debugLevelElt : _logLevelMap) {
        QString debugLevelStr = debugLevelElt.second.second;
        if (debugLevelElt.second.first == 0) {
            debugLevelStr = debugLevelElt.second.second + QString::fromStdString(" (Recommended)");
        }
        _debugLevelComboBox->insertItem(debugLevelElt.second.first, debugLevelElt.second.second,
                                        enumClassToInt(debugLevelElt.first));
    }
    debugLevelSelectionHBox->addWidget(_debugLevelComboBox);

    debugLevelSelectionHBox->addSpacing(boxHSpacing);

    // Debug info main box | Debug Level Main Box | Debug level select box | Extended Full Log checkbox
    QString extendedLogCheckBoxToolTip =
        tr("The extended full log collects a detailed history that can be used for debugging. Enabling it can slow down the "
           "kDrive application.");
    _extendedLogCheckBox = new CustomCheckBox();
    _extendedLogCheckBox->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    _extendedLogCheckBox->setText(tr("Extended Full Log"));
    _extendedLogCheckBox->setToolTip(extendedLogCheckBoxToolTip);
    debugLevelSelectionHBox->addWidget(_extendedLogCheckBox);

    // Debug info main box | Debug Level Main Box | Debug level select box | Helper
    _extendedLogHelpButton = new CustomToolButton();
    _extendedLogHelpButton->setObjectName("helpButton");
    _extendedLogHelpButton->setIconPath(":/client/resources/icons/actions/help.svg");
    _extendedLogHelpButton->setFixedSize(15, 15);
    _extendedLogHelpButton->setEnabled(false);
    _extendedLogHelpButton->setToolTip(extendedLogCheckBoxToolTip);
    _extendedLogHelpButton->setToolTipDuration(20000);
    debugLevelSelectionHBox->addWidget(_extendedLogHelpButton);

    debuggingInfoMainHBox->addSpacing(boxHSpacing);

    // Debug info main box | Delete logs | Main box
    QHBoxLayout *deleteLogsHBox = new QHBoxLayout();
    debuggingInfoMainHBox->addLayout(deleteLogsHBox);

    // Debug info main box | Delete logs | Main box | Checkbox
    _deleteLogsCheckBox = new CustomCheckBox();
    _deleteLogsCheckBox->setObjectName("deleteLogsCheckBox");
    _deleteLogsCheckBox->setText(tr("Delete logs older than %1 days").arg(CommonUtility::logsPurgeRate));
    deleteLogsHBox->addWidget(_deleteLogsCheckBox);

    debuggingInfoMainHBox->addSpacing(boxHMargin);

    // Debug info main box | Log upload | Main box
    QFrame *logUploadFrame = new QFrame();
    logUploadFrame->setObjectName("logUploadFrame");
    logUploadFrame->setStyleSheet("QFrame#logUploadFrame {border-radius: 8px; border: 2px solid #E0E0E0;}");
    logUploadFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QVBoxLayout *logUploadMainBox = new QVBoxLayout(logUploadFrame);
    logUploadMainBox->setContentsMargins(boxHMargin / 2, 0, boxHMargin / 2, 0);
    logUploadMainBox->addSpacing(recordDebuggingBoxVMargin);
    debuggingInfoMainHBox->addWidget(logUploadFrame);


    // Log upload | Main box | Label (largeNormalBoldTextLabel)
    QLabel *logUploadLabel = new QLabel();
    logUploadLabel->setObjectName("largeNormalBoldTextLabel");
    logUploadLabel->setText(tr("Share the debug folder with Infomaniak support."));
    logUploadMainBox->addWidget(logUploadLabel);

    // Log upload | Main box | heavy log box
    _heavyLogBox = new QWidget();
    QVBoxLayout *heavyLogHBox = new QVBoxLayout(_heavyLogBox);
    heavyLogHBox->setContentsMargins(0, 0, 0, 0);
    logUploadMainBox->addWidget(_heavyLogBox);
    _heavyLogBox->hide();

    // Log upload | Main box | heavy log box | Label (normalTextLabel)
    _heavyLogLabel = new QLabel();
    _heavyLogLabel->setObjectName("largeNormalTextLabel");
    _heavyLogLabel->setText(heavyLogLabelStr);
    _heavyLogBox->hide();  // show only if the log dir is large, see at the end of the function
    _heavyLogLabel->setWordWrap(true);
    heavyLogHBox->addWidget(_heavyLogLabel);
    heavyLogHBox->addSpacing(recordDebuggingBoxVMargin);

    // Log upload | Main box | heavy log box | Checkbox box
    QHBoxLayout *heavyLogCheckBoxVBox = new QHBoxLayout();
    heavyLogCheckBoxVBox->setAlignment(Qt::AlignLeft);
    heavyLogHBox->addLayout(heavyLogCheckBoxVBox);

    QString heavyLogCheckBoxToolTip = tr("The last session is the periode since the last kDrive start.");
    // Log upload | Main box | heavy log box | Checkbox box | Checkbox
    CustomCheckBox *heavyLogCheckBox = new CustomCheckBox();
    heavyLogCheckBox->setObjectName("heavyLogCheckBox");
    heavyLogCheckBox->setText(tr("Share only the last kDrive session"));
    heavyLogCheckBox->setChecked(true);
    heavyLogCheckBox->setToolTip(heavyLogCheckBoxToolTip);
    heavyLogCheckBox->setToolTipDuration(20000);
    heavyLogCheckBoxVBox->addWidget(heavyLogCheckBox);
    _sendArchivedLogs = !heavyLogCheckBox->isChecked();
    // Log upload | Main box | heavy log box | Checkbox box | Helper
    CustomToolButton *heavyLogHelpButton = new CustomToolButton();
    heavyLogHelpButton->setObjectName("helpButton");
    heavyLogHelpButton->setIconPath(":/client/resources/icons/actions/help.svg");
    heavyLogHelpButton->setFixedSize(15, 15);
    heavyLogHelpButton->setEnabled(false);
    heavyLogHelpButton->setToolTip(heavyLogCheckBoxToolTip);
    heavyLogHelpButton->setToolTipDuration(20000);
    heavyLogCheckBoxVBox->addWidget(heavyLogHelpButton);

    // Log upload | Main box | info box
    _logUploadInfoWidget = new QWidget();
    _logUploadInfoHBox = new QHBoxLayout(_logUploadInfoWidget);  // Will be filed depending on the log upload status
    logUploadMainBox->addWidget(_logUploadInfoWidget);

    logUploadMainBox->addSpacing(boxHSpacing);

    // Log upload | Main box | shared button
    _sendLogButton = new QPushButton();
    _sendLogButton->setObjectName("defaultbutton");
    _sendLogButton->setFlat(true);
    _sendLogButton->setEnabled(false);
    _sendLogButton->setIcon(
        KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/share.svg", QColor(255, 255, 255)));
    _sendLogButton->setIconSize(QSize(16, 16));
    _sendLogButton->setText(tr("  Loading"));
    logUploadMainBox->addWidget(_sendLogButton);
    logUploadMainBox->addSpacing(boxHSpacing);


    // Log upload | Main box | cancel button
    _cancelLogUploadButton = new QPushButton();
    _cancelLogUploadButton->setObjectName("nondefaultbutton");
    _cancelLogUploadButton->setFlat(true);
    _cancelLogUploadButton->setText(tr("Cancel"));
    _cancelLogUploadButton->setEnabled(false);
    _cancelLogUploadButton->hide();
    logUploadMainBox->addWidget(_cancelLogUploadButton);

    logUploadMainBox->addSpacing(recordDebuggingBoxVMargin);

    debuggingInfoMainHBox->addStretch();

    // Scrollable main widget | Scroll area
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidget(scrollableMainWidget);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    mainLayout->addWidget(scrollArea);
    mainLayout->addSpacing(boxHSpacing);
    // Add dialog buttons
    QHBoxLayout *buttonsHBox = new QHBoxLayout();
    buttonsHBox->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    buttonsHBox->setSpacing(boxHSpacing);
    mainLayout->addLayout(buttonsHBox);

    _saveButton = new QPushButton();
    _saveButton->setObjectName("defaultbutton");
    _saveButton->setFlat(true);
    _saveButton->setText(tr("SAVE"));
    _saveButton->setEnabled(false);
    buttonsHBox->addWidget(_saveButton);

    QPushButton *cancelButton = new QPushButton();
    cancelButton->setObjectName("nondefaultbutton");
    cancelButton->setFlat(true);
    cancelButton->setText(tr("CANCEL"));
    buttonsHBox->addWidget(cancelButton);
    buttonsHBox->addStretch();

    connect(_recordDebuggingSwitch, &CustomSwitch::clicked, this, &DebuggingDialog::onRecordDebuggingSwitchClicked);
    connect(_debuggingFolderLabel, &QLabel::linkActivated, this, &DebuggingDialog::onLinkActivated);
    connect(_debugLevelComboBox, QOverload<int>::of(&QComboBox::activated), this,
            &DebuggingDialog::onDebugLevelComboBoxActivated);
    connect(_deleteLogsCheckBox, &CustomCheckBox::clicked, this, &DebuggingDialog::onDeleteLogsCheckBoxClicked);
    connect(_extendedLogCheckBox, &CustomCheckBox::clicked, this, &DebuggingDialog::onExtendedLogCheckBoxClicked);
    connect(heavyLogCheckBox, &CustomCheckBox::clicked, this, &DebuggingDialog::onSendArchivedLogsCheckBoxClicked);
    connect(_sendLogButton, &QPushButton::clicked, this, &DebuggingDialog::onSendLogButtonTriggered);
    connect(_saveButton, &QPushButton::clicked, this, &DebuggingDialog::onSaveButtonTriggered);
    connect(_cancelLogUploadButton, &QPushButton::clicked, this, &DebuggingDialog ::onCancelLogUploadButtonTriggered);
    connect(cancelButton, &QPushButton::clicked, this, &DebuggingDialog::onExit);
    connect(_gui.get(), &ClientGui::logUploadStatusUpdated, this, &DebuggingDialog::onLogUploadStatusUpdated);


    connect(this, &CustomDialog::exit, this, &DebuggingDialog::onExit);

    QTimer::singleShot(0, this, [this]() { initLogUploadLayout(); });
}

void DebuggingDialog::initLogUploadLayout() {
    AppStateValue appStateValue = LogUploadState::None;
    LogUploadState logUploadState = LogUploadState::None;
    ExitCode exitCode = GuiRequests::getAppState(AppStateKey::LogUploadState, appStateValue);
    if (exitCode == ExitCode::Ok) {
        logUploadState = std::get<LogUploadState>(appStateValue);
    } else {
        qCWarning(lcDebuggingDialog) << "Failed to get log upload status";
    }

    if (logUploadState == LogUploadState::Canceled) {  // If the last log upload was canceled, we do not show the cancel label
                                                       // when reopening the dialog
        appStateValue = std::string();
        QString LastSuccessfulLogUploadDate = "";
        exitCode = GuiRequests::getAppState(AppStateKey::LastSuccessfulLogUploadDate, appStateValue);
        if (exitCode == ExitCode::Ok) {
            LastSuccessfulLogUploadDate = QString::fromStdString(std::get<std::string>(appStateValue));
        } else {
            qCWarning(lcDebuggingDialog) << "Failed to get last successful log upload date";
        }

        if (LastSuccessfulLogUploadDate.size() == 0) {
            logUploadState = LogUploadState::None;
        } else {
            logUploadState = LogUploadState::Success;
        }
    }

    appStateValue = int();
    int logUploadPercent = 0;
    exitCode = GuiRequests::getAppState(AppStateKey::LogUploadPercent, appStateValue);
    if (exitCode == ExitCode::Ok) {
        logUploadPercent = std::get<int>(appStateValue);
    } else {
        qCWarning(lcDebuggingDialog) << "Failed to get log upload percent";
    }
    onLogUploadStatusUpdated(logUploadState, logUploadPercent);
}

void DebuggingDialog::displayHeavyLogBox() {
    bool sendLogButtonEnabled = _sendLogButton->isEnabled();
    QString sendLogButtonText = _sendLogButton->text();
    _sendLogButton->setText("  Loading...");
    _sendLogButton->setEnabled(false);

    uint64_t logDirSize = 0;
    ExitCode exitCode = GuiRequests::getLogDirEstimatedSize(logDirSize);
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcDebuggingDialog) << "Failed to get log dir estimated size";
    }
    if (logDirSize > 100000000) {
        _heavyLogBox->setVisible(true);
    } else {
        _heavyLogBox->setVisible(false);
        _sendLogButton->setText(sendLogButtonText);
        _sendLogButton->setEnabled(sendLogButtonEnabled);
        return;
    }
    _heavyLogLabel->setText(heavyLogLabelStr);
    _sendLogButton->setText(sendLogButtonText);
    _sendLogButton->setEnabled(sendLogButtonEnabled);
}

void DebuggingDialog::setlogUploadInfo(LogUploadState status) {
    while (_logUploadInfoHBox->count() > 0) {
        _logUploadInfoHBox->itemAt(0)->widget()->deleteLater();
        _logUploadInfoHBox->removeItem(_logUploadInfoHBox->itemAt(0));
    }

    QString archivePathStr;
    QString lasSuccessfullUploadDate;

    AppStateValue appStateValue = std::string();
    ExitCode exitcode = GuiRequests::getAppState(AppStateKey::LastLogUploadArchivePath, appStateValue);
    if (exitcode == ExitCode::Ok) {
        archivePathStr = QString::fromStdString(std::get<std::string>(appStateValue));
    } else {
        qCWarning(lcDebuggingDialog) << "Failed to get last log upload archive path";
    }

    appStateValue = std::string();
    exitcode = GuiRequests::getAppState(AppStateKey::LastSuccessfulLogUploadDate, appStateValue);
    if (exitcode == ExitCode::Ok) {
        lasSuccessfullUploadDate =
            convertAppStateTimeToLocalHumanReadable(QString::fromStdString(std::get<std::string>(appStateValue)));
    } else {
        qCWarning(lcDebuggingDialog) << "Failed to get last successful log upload date";
    }

    if (status == LogUploadState::Uploading || status == LogUploadState::Archiving || status == LogUploadState::None) {
        return;
    }

    if (status == LogUploadState::Failed) {
        QFrame *errorFrame = new QFrame();
        errorFrame->setObjectName("errorFrame");
        errorFrame->setStyleSheet(
            "QFrame#errorFrame {border-radius: 8px;  border: 2px solid #E0E0E0; background-color: #F8F8F8;}");

        QVBoxLayout *errorHBox = new QVBoxLayout(errorFrame);
        errorHBox->setAlignment(Qt::AlignLeft);
        errorHBox->setContentsMargins(10, 10, 10, 10);

        _logUploadInfoHBox->addWidget(errorFrame);

        QHBoxLayout *errorTitleHBox = new QHBoxLayout();
        // take all the space it needs
        errorTitleHBox->setAlignment(Qt::AlignLeft);
        errorTitleHBox->setContentsMargins(0, 0, 0, 0);
        errorHBox->addLayout(errorTitleHBox);

        QLabel *warningIconLabel = new QLabel();
        warningIconLabel->setPixmap(
            KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/warning.svg", QColor(255, 0, 0)).pixmap(20, 20));

        errorTitleHBox->addWidget(warningIconLabel);

        QLabel *errorTitleLabel = new QLabel();
        errorTitleLabel->setObjectName("largeNormalBoldTextLabel");

        QString errorText = tr("Failed to share");
        QLabel *errorLabel = new QLabel();
        errorTitleLabel->setWordWrap(false);
        errorTitleLabel->setMinimumHeight(20);
        if (archivePathStr.isEmpty()) {  // We don't pass the archiving step, most likely because there is no drive setup
            errorTitleLabel->setText(errorText);
            errorTitleHBox->addWidget(errorTitleLabel);
            errorLabel->setObjectName("largeNormalTextLabel");
            errorLabel->setTextFormat(Qt::RichText);
            errorLabel->setContentsMargins(25, 5, 0, 0);
            errorLabel->setText(tr("1. Check that you are logged in <br>2. Check that you have configured at least one kDrive"));
            errorHBox->addWidget(errorLabel);

        } else {
            SyncPath archivePath = archivePathStr.toStdString();
            errorTitleLabel->setText(errorText + tr(" (Connexion interrupted)"));
            errorTitleHBox->addWidget(errorTitleLabel);

            errorLabel->setTextFormat(Qt::RichText);
            errorLabel->setObjectName("largeNormalTextLabel");
            errorLabel->setContentsMargins(25, 5, 0, 0);
            errorLabel->setText(tr("Share the folder with SwissTransfer <br>") +
                                tr(" 1. We automatically compressed your log <a style=\"%1\" href=\"%2\">here</a>.<br>")
                                    .arg(CommonUtility::linkStyle, QString::fromStdString(archivePath.parent_path().string())) +
                                tr(" 2. Transfer the archive with <a style=\"%1\" href=\"%2\">swisstransfer.com</a><br>")
                                    .arg(CommonUtility::linkStyle, "https://www.swisstransfer.com/") +
                                tr(" 3. Share the link with <a style=\"%1\" href=\"%2\"> support@infomaniak.com </a><br>")
                                    .arg(CommonUtility::linkStyle, "mailto:support@infomaniak.com"));
            errorLabel->setToolTip(QString::fromStdString(archivePath.parent_path().string()));
            errorHBox->addWidget(errorLabel);
            connect(errorLabel, &QLabel::linkActivated, this, &DebuggingDialog::onLinkActivated);
        }
        return;
    }

    QFrame *lastUploadFrame = new QFrame();

    lastUploadFrame->setObjectName("lastUploadFrame");
    lastUploadFrame->setStyleSheet(
        "QFrame#lastUploadFrame {border-radius: 8px; border: 1px solid #2C8736; background-color: #E0F8E2;}");

    QVBoxLayout *lastUploadHBox = new QVBoxLayout(lastUploadFrame);
    lastUploadHBox->setAlignment(Qt::AlignLeft);
    lastUploadHBox->setContentsMargins(3, 3, 3, 3);

    QHBoxLayout *lastUploadTitleHBox = new QHBoxLayout(lastUploadFrame);
    lastUploadTitleHBox->setAlignment(Qt::AlignLeft);
    lastUploadHBox->addLayout(lastUploadTitleHBox);

    QLabel *checkIconLabel = new QLabel();
    checkIconLabel->setPixmap(
        KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/check.svg", QColor(44, 135, 54)).pixmap(16, 16));
    lastUploadTitleHBox->addWidget(checkIconLabel);

    QLabel *lastUploadTitleLabel = new QLabel();
    lastUploadTitleLabel->setObjectName("largeNormalBoldTextLabel");
    lastUploadTitleLabel->setText(tr("Last upload the %1").arg(lasSuccessfullUploadDate));
    lastUploadTitleLabel->setStyleSheet("QLabel {color: #2C8736;}");
    lastUploadTitleHBox->addWidget(lastUploadTitleLabel);

    if (status == LogUploadState::Canceled) {
        QFrame *cancelFrame = new QFrame();

        cancelFrame->setObjectName("cancelFrame");
        cancelFrame->setStyleSheet("QFrame#cancelFrame {border-radius: 8px; background-color: #FFF5D3;}");
        _logUploadInfoHBox->addWidget(cancelFrame);

        QVBoxLayout *cancelHBox = new QVBoxLayout(cancelFrame);
        cancelHBox->setAlignment(Qt::AlignLeft);
        cancelHBox->setContentsMargins(3, 3, 3, 3);

        QHBoxLayout *cancelTitleHBox = new QHBoxLayout(cancelFrame);
        cancelTitleHBox->setAlignment(Qt::AlignLeft);
        cancelHBox->addLayout(cancelTitleHBox);

        QLabel *warningIconLabel = new QLabel();
        warningIconLabel->setPixmap(
            KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/warning.svg", QColor(204, 97, 2)).pixmap(16, 16));
        cancelTitleHBox->addWidget(warningIconLabel);

        QLabel *cancelTitleLabel = new QLabel();
        cancelTitleLabel->setObjectName("largeNormalBoldTextLabel");
        cancelTitleLabel->setStyleSheet("QLabel {color: #CC6102;}");
        cancelTitleLabel->setText(tr("Sharing has been cancelled"));
        cancelTitleHBox->addWidget(cancelTitleLabel);

        if (!lasSuccessfullUploadDate.isEmpty() && lasSuccessfullUploadDate.toStdString() != "0") {
            _logUploadInfoHBox->addWidget(lastUploadFrame);
        }
    } else if (status == LogUploadState::Success && lasSuccessfullUploadDate.toStdString() != "0") {
        _logUploadInfoHBox->addWidget(lastUploadFrame);
    }
    return;
}

void DebuggingDialog::updateUI() {
    _recordDebuggingSwitch->setCheckState(_recordDebugging ? Qt::Checked : Qt::Unchecked);
    _extendedLogCheckBox->setCheckState(_extendedLog ? Qt::Checked : Qt::Unchecked);
    _debuggingInfoMainWidget->setVisible(_recordDebugging);
    _debugLevelComboBox->setEnabled(_recordDebugging);
    _debugLevelComboBox->setCurrentIndex(_recordDebugging ? enumClassToInt(_minLogLevel) : -1);

    _deleteLogsCheckBox->setEnabled(_recordDebugging);
    _deleteLogsCheckBox->setChecked(_recordDebugging ? _deleteLogs : false);

    if (_minLogLevel != LogLevel::Debug) {
        _extendedLogCheckBox->hide();
        _extendedLogHelpButton->hide();
        _extendedLog = false;
    } else {
        _extendedLogCheckBox->show();
        _extendedLogHelpButton->show();
    }
}

void DebuggingDialog::setNeedToSave(bool value) {
    _needToSave = value;
    _saveButton->setEnabled(value);
}

QString DebuggingDialog::convertAppStateTimeToLocalHumanReadable(const QString &time) const {
    // App state time is formatted as "month,day,year,hour,minute,second"
    QStringList timeParts = time.split(',');
    if (timeParts.size() != 6) {
        // Invalid time format 
        return time;
    }

    //: Date format for the last successful log upload. %1: month, %2: day, %3: year, %4: hour, %5: minute, %6: second
    return tr("%1/%2/%3 at %4h%5m and %6s").arg(timeParts[0], timeParts[1], timeParts[2], timeParts[3], timeParts[4], timeParts[5]);
}

void DebuggingDialog::onRecordDebuggingSwitchClicked(bool checked) {
    _recordDebugging = checked;
    _debuggingInfoMainWidget->setVisible(checked);
    updateUI();
    setNeedToSave(true);
}

void DebuggingDialog::onExtendedLogCheckBoxClicked(bool checked) {
    _extendedLog = checked;
    updateUI();
    setNeedToSave(true);
}

void DebuggingDialog::onDebugLevelComboBoxActivated(int index) {
    _minLogLevel = qvariant_cast<LogLevel>(_debugLevelComboBox->itemData(index));
    updateUI();
    setNeedToSave(true);
}

void DebuggingDialog::onDeleteLogsCheckBoxClicked(bool checked) {
    _deleteLogs = checked;
    setNeedToSave(true);
}

void DebuggingDialog::onSendArchivedLogsCheckBoxClicked(bool checked) {
    _sendArchivedLogs = !checked;
}

void DebuggingDialog::onExit() {
    EnableStateHolder _(this);

    if (_needToSave) {
        CustomMessageBox msgBox(QMessageBox::Question, tr("Do you want to save your modifications?"),
                                QMessageBox::Yes | QMessageBox::No, this);
        msgBox.setDefaultButton(QMessageBox::Yes);
        int ret = msgBox.exec();
        if (ret != QDialog::Rejected) {
            if (ret == QMessageBox::Yes) {
                onSaveButtonTriggered();
            } else {
                reject();
            }
        }
    } else {
        reject();
    }
}

void DebuggingDialog::onSendLogButtonTriggered() {
    _sendLogButton->setEnabled(false);
    GuiRequests::sendLogToSupport(_sendArchivedLogs || _heavyLogBox->isHidden());
}

void DebuggingDialog::onCancelLogUploadButtonTriggered() {
    _cancelLogUploadButton->setEnabled(false);
    GuiRequests::cancelLogUploadToSupport();
}

void DebuggingDialog::onSaveButtonTriggered(bool checked) {
    Q_UNUSED(checked)

    ParametersCache::instance()->parametersInfo().setUseLog(_recordDebugging);
    ParametersCache::instance()->parametersInfo().setExtendedLog(_extendedLog);
    ParametersCache::instance()->parametersInfo().setLogLevel(_minLogLevel);
    ParametersCache::instance()->parametersInfo().setPurgeOldLogs(_deleteLogs);
    ParametersCache::instance()->saveParametersInfo();

    accept();
}

void DebuggingDialog::onLinkActivated(const QString &link) {
    QString folderPath;
    if (link == debuggingFolderLink) {
        folderPath = Logger::instance()->temporaryFolderLogDirPath();
    } else {
        folderPath = link;
    }
    QUrl debuggingFolderUrl = KDC::GuiUtility::getUrlFromLocalPath(folderPath);
    if (debuggingFolderUrl.isValid()) {
        if (!QDesktopServices::openUrl(debuggingFolderUrl)) {
            qCWarning(lcDebuggingDialog) << "QDesktopServices::openUrl failed for " << debuggingFolderUrl.toString();
            CustomMessageBox msgBox(QMessageBox::Warning, tr("Unable to open folder %1.").arg(debuggingFolderUrl.toString()),
                                    QMessageBox::Ok, this);
            msgBox.exec();
        }
    } else {
        qCWarning(lcDebuggingDialog) << "QDesktopServices::openUrl failed for " << debuggingFolderUrl.toString();
        CustomMessageBox msgBox(QMessageBox::Warning, tr("Unable to open folder %1.").arg(debuggingFolderUrl.toString()),
                                QMessageBox::Ok, this);
        msgBox.exec();
    }
}

void DebuggingDialog::onLogUploadStatusUpdated(LogUploadState state, int progress) {
    QString shareText = tr("  Share");
    switch (state) {
        case LogUploadState::None:
            _sendLogButton->setText(shareText);
            _sendLogButton->setEnabled(true);
            _logUploadInfoWidget->hide();
            _cancelLogUploadButton->hide();
            displayHeavyLogBox();
            break;
        case LogUploadState::Archiving:
            _sendLogButton->setText(tr("  Sharing | step 1/2 %1%").arg(progress));
            _sendLogButton->setEnabled(false);
            setlogUploadInfo(state);
            _logUploadInfoWidget->show();
            _heavyLogBox->hide();
            _cancelLogUploadButton->show();
            _cancelLogUploadButton->setEnabled(true);
            break;
        case LogUploadState::Uploading:
            _sendLogButton->setText(tr("  Sharing | step 2/2 %1%").arg(progress));
            _sendLogButton->setEnabled(false);
            setlogUploadInfo(state);
            _logUploadInfoWidget->show();
            _heavyLogBox->hide();
            _cancelLogUploadButton->show();
            _cancelLogUploadButton->setEnabled(true);
            break;
        case LogUploadState::CancelRequested:
            if (progress == 0) {
                _sendLogButton->setText(tr("  Canceling..."));
                _sendLogButton->setEnabled(false);
                _cancelLogUploadButton->hide();
                _cancelLogUploadButton->setEnabled(false);
            }
            break;
        case LogUploadState::Canceled:
        case LogUploadState::Failed:
        case LogUploadState::Success:
            _sendLogButton->setText(shareText);
            _sendLogButton->setEnabled(true);
            setlogUploadInfo(state);
            _logUploadInfoWidget->show();
            _cancelLogUploadButton->hide();
            displayHeavyLogBox();
            break;
        default:
            break;
    }
}

}  // namespace KDC
