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

#include "customdialog.h"
#include "customswitch.h"
#include "customcombobox.h"
#include "customcheckbox.h"
#include "libcommon/utility/types.h"
#include "libcommon/info/parametersinfo.h"

#include <QPushButton>
#include <QLabel>

namespace KDC {

class ClientGui;

class DebuggingDialog : public CustomDialog {
        Q_OBJECT

    public:
        explicit DebuggingDialog(std::shared_ptr<ClientGui> _gui, QWidget *parent = nullptr);

    private:
        std::shared_ptr<ClientGui> _gui;
        static std::map<LogLevel, std::pair<int, QString>> _logLevelMap;

        CustomSwitch *_recordDebuggingSwitch;
        QWidget *_debuggingInfoMainWidget;
        CustomComboBox *_debugLevelComboBox;
        CustomCheckBox *_extendedLogCheckBox;
        CustomCheckBox *_deleteLogsCheckBox;
        QPushButton *_saveButton;
        QPushButton *_sendLogButton;
        QPushButton *_cancelLogUploadButton;
        QLabel *_debuggingFolderLabel;
        QWidget *_heavyLogBox;
        QLabel *_heavyLogLabel;
        QWidget *_logUploadInfoWidget;
        QHBoxLayout *_logUploadInfoHBox;
        bool _recordDebugging;
        bool _extendedLog;
        LogLevel _minLogLevel;
        bool _deleteLogs;
        bool _needToSave;
        bool _sendArchivedLogs = true;

        void initUI();
        void initLogUploadLayout();
        void displayHeavyLogBox();
        void setlogUploadInfo(LogUploadState status);
        void updateUI();
        void setNeedToSave(bool value);

    private slots:
        void onRecordDebuggingSwitchClicked(bool checked = false);
        void onExtendedLogCheckBoxClicked(bool checked = false);
        void onDebugLevelComboBoxActivated(int index);
        void onDeleteLogsCheckBoxClicked(bool checked = false);
        void onSendArchivedLogsCheckBoxClicked(bool checked = false);
        void onExit();
        void onSendLogButtonTriggered(bool checked = false);
        void onCancelLogUploadButtonTriggered(bool checked = false);
        void onSaveButtonTriggered(bool checked = false);
        void onLinkActivated(const QString &link);
        void onLogUploadStatusUpdated(LogUploadState state, int progress);
};

}  // namespace KDC
