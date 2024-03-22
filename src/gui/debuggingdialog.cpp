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
#include "libcommon/utility/types.h"
#include "libcommon/comm.h"
#include "libcommongui/commclient.h"

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

std::map<LogLevel, std::pair<int, QString>> DebuggingDialog::_logLevelMap = {{LogLevelDebug, {0, QString(tr("Debug"))}},
                                                                             {LogLevelInfo, {1, QString(tr("Info"))}},
                                                                             {LogLevelWarning, {2, QString(tr("Warning"))}},
                                                                             {LogLevelError, {3, QString(tr("Error"))}},
                                                                             {LogLevelFatal, {4, QString(tr("Fatal"))}}};

DebuggingDialog::DebuggingDialog(std::shared_ptr<ClientGui> gui, QWidget *parent)
    : CustomDialog(true, parent),
      _gui(gui),
      _recordDebuggingSwitch(nullptr),
      _debugLevelComboBox(nullptr),
      _extendedLogCheckBox(nullptr),
      _saveButton(nullptr),
      _recordDebugging(false),
      _extendedLog(false),
      _minLogLevel(LogLevelInfo),
      _deleteLogs(false),
      _needToSave(false) {
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
    QLabel *titleLabel = new QLabel(this);
    titleLabel->setObjectName("titleLabel");
    titleLabel->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    titleLabel->setText(tr("Record debugging information"));
    mainLayout->addWidget(titleLabel);
    mainLayout->addSpacing(titleBoxVMargin);

    // Record debugging information
    QHBoxLayout *recordDebuggingHBox = new QHBoxLayout();
    recordDebuggingHBox->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    mainLayout->addLayout(recordDebuggingHBox);
    mainLayout->addSpacing(recordDebuggingBoxVMargin);

    QLabel *recordDebuggingLabel = new QLabel(this);
    recordDebuggingLabel->setObjectName("boldTextLabel");
    recordDebuggingLabel->setText(tr("Activate the recording of information in a temporary folder"));
    recordDebuggingHBox->addWidget(recordDebuggingLabel);
    recordDebuggingHBox->addStretch();

    _recordDebuggingSwitch = new CustomSwitch(this);
    _recordDebuggingSwitch->setLayoutDirection(Qt::RightToLeft);
    _recordDebuggingSwitch->setAttribute(Qt::WA_MacShowFocusRect, false);
    recordDebuggingHBox->addWidget(_recordDebuggingSwitch);

    // Minimum debug level
    QLabel *debugLevelLabel = new QLabel(this);
    debugLevelLabel->setObjectName("boldTextLabel");
    debugLevelLabel->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    debugLevelLabel->setText(tr("Minimum trace level"));
    mainLayout->addWidget(debugLevelLabel);
    mainLayout->addSpacing(debugLevelLabelBoxVMargin);

    QHBoxLayout *debugLevelHBox = new QHBoxLayout();
    debugLevelHBox->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    mainLayout->addLayout(debugLevelHBox);
    mainLayout->addSpacing(debugLevelSelectBoxVMargin);

    _debugLevelComboBox = new CustomComboBox(this);
    _debugLevelComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    _debugLevelComboBox->setAttribute(Qt::WA_MacShowFocusRect, false);
    for (auto const &debugLevelElt : _logLevelMap) {
        _debugLevelComboBox->insertItem(debugLevelElt.second.first, debugLevelElt.second.second, debugLevelElt.first);
    }
    debugLevelHBox->addWidget(_debugLevelComboBox);
    debugLevelHBox->addSpacing(debugLevelSelectBoxVMargin);

    _extendedLogCheckBox = new CustomCheckBox(this);
    _extendedLogCheckBox->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    _extendedLogCheckBox->setText(tr("Extended Full Log"));
    _extendedLogCheckBox->setToolTip(tr("This feature can slow down the application"));
    debugLevelHBox->addWidget(_extendedLogCheckBox);

    debugLevelHBox->addStretch();

    // Delete logs
    QHBoxLayout *deleteLogsHBox = new QHBoxLayout();
    deleteLogsHBox->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    mainLayout->addLayout(deleteLogsHBox);

    _deleteLogsCheckBox = new CustomCheckBox(this);
    _deleteLogsCheckBox->setObjectName("deleteLogsCheckBox");
    _deleteLogsCheckBox->setText(tr("Delete logs older than %1 days").arg(ClientGui::logsPurgeRate()));
    deleteLogsHBox->addWidget(_deleteLogsCheckBox);

    mainLayout->addStretch();

    // Add dialog buttons
    QHBoxLayout *buttonsHBox = new QHBoxLayout();
    buttonsHBox->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    buttonsHBox->setSpacing(boxHSpacing);
    mainLayout->addLayout(buttonsHBox);

    _saveButton = new QPushButton(this);
    _saveButton->setObjectName("defaultbutton");
    _saveButton->setFlat(true);
    _saveButton->setText(tr("SAVE"));
    _saveButton->setEnabled(false);
    buttonsHBox->addWidget(_saveButton);

    QPushButton *cancelButton = new QPushButton(this);
    cancelButton->setObjectName("nondefaultbutton");
    cancelButton->setFlat(true);
    cancelButton->setText(tr("CANCEL"));
    buttonsHBox->addWidget(cancelButton);
    buttonsHBox->addStretch();

    connect(_recordDebuggingSwitch, &CustomSwitch::clicked, this, &DebuggingDialog::onRecordDebuggingSwitchClicked);
    connect(_debugLevelComboBox, QOverload<int>::of(&QComboBox::activated), this,
            &DebuggingDialog::onDebugLevelComboBoxActivated);
    connect(_deleteLogsCheckBox, &CustomCheckBox::clicked, this, &DebuggingDialog::onDeleteLogsCheckBoxClicked);
    connect(_extendedLogCheckBox, &CustomCheckBox::clicked, this, &DebuggingDialog::onExtendedLogCheckBoxClicked);
    connect(_saveButton, &QPushButton::clicked, this, &DebuggingDialog::onSaveButtonTriggered);
    connect(cancelButton, &QPushButton::clicked, this, &DebuggingDialog::onExit);
    connect(this, &CustomDialog::exit, this, &DebuggingDialog::onExit);
}

void DebuggingDialog::updateUI() {
    _recordDebuggingSwitch->setCheckState(_recordDebugging ? Qt::Checked : Qt::Unchecked);
    _extendedLogCheckBox->setCheckState(_extendedLog ? Qt::Checked : Qt::Unchecked);

    _debugLevelComboBox->setEnabled(_recordDebugging);
    _debugLevelComboBox->setCurrentIndex(_recordDebugging ? _minLogLevel : -1);

    _deleteLogsCheckBox->setEnabled(_recordDebugging);
    _deleteLogsCheckBox->setChecked(_recordDebugging ? _deleteLogs : false);

    if (_minLogLevel != LogLevel::LogLevelDebug) {
        _extendedLogCheckBox->hide();
        _extendedLog = false;
    } else {
        _extendedLogCheckBox->show();
    }
}

void DebuggingDialog::setNeedToSave(bool value) {
    _needToSave = value;
    _saveButton->setEnabled(value);
}

void DebuggingDialog::onRecordDebuggingSwitchClicked(bool checked) {
    _recordDebugging = checked;
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

void DebuggingDialog::onSaveButtonTriggered(bool checked) {
    Q_UNUSED(checked)

    ParametersCache::instance()->parametersInfo().setUseLog(_recordDebugging);
    ParametersCache::instance()->parametersInfo().setExtendedLog(_extendedLog);
    ParametersCache::instance()->parametersInfo().setLogLevel(_minLogLevel);
    ParametersCache::instance()->parametersInfo().setPurgeOldLogs(_deleteLogs);
    ParametersCache::instance()->saveParametersInfo();

    accept();
}

}  // namespace KDC
