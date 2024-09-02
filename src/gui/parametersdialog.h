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
#include "info/syncfileiteminfo.h"
#include "info/errorinfo.h"
#include "mainmenubarwidget.h"
#include "preferencesmenubarwidget.h"
#include "errorsmenubarwidget.h"
#include "drivepreferenceswidget.h"
#include "preferenceswidget.h"

#include <QByteArray>
#include <QColor>
#include <QDialog>
#include <QListWidget>
#include <QStackedWidget>
#include <QString>
#include <QTimer>

namespace KDC {

class ClientGui;

class ParametersDialog : public CustomDialog {
        Q_OBJECT

    public:
        explicit ParametersDialog(std::shared_ptr<ClientGui> gui, QWidget *parent = nullptr);

        void openPreferencesPage();
        void openGeneralErrorsPage();
        void openDriveParametersPage(int driveDbId);
        void openDriveErrorsPage(int driveDbId);

        void resolveConflictErrors(int driveDbId, bool keepLocalVersion);
        void resolveUnsupportedCharErrors(int driveDbId);

    signals:
        void addDrive();
        void setStyle(bool darkTheme);
        void newBigFolder(int syncDbId, const QString &path);
        void removeDrive(int driveDbId);
        void removeSync(int syncDbId);
        void executeSyncAction(ActionType type, ActionTarget target, int dbId);
        void clearErrors(int syncDbId, bool autoResolved);

    public slots:
        void onConfigRefreshed();
        void onUpdateProgress(int syncDbId);
        void onItemCompleted(int syncDbId, const SyncFileItemInfo &item);
        void onDriveQuotaUpdated(int driveDbId);
        void onRefreshErrorList(int driveDbId);
        void onRefreshStatusNeeded();

    private:
        enum Page { Drive = 0, Preferences, Errors };

        std::shared_ptr<ClientGui> _gui;
        int _currentDriveDbId{0};

        // General errors
        int _errorTabWidgetStackPosition{0};

        QColor _backgroundMainColor;
        QStackedWidget *_pageStackedWidget{nullptr};
        MainMenuBarWidget *_driveMenuBarWidget{nullptr};
        PreferencesMenuBarWidget *_preferencesMenuBarWidget{nullptr};
        ErrorsMenuBarWidget *_errorsMenuBarWidget{nullptr};
        PreferencesWidget *_preferencesWidget{nullptr};
        DrivePreferencesWidget *_drivePreferencesWidget{nullptr};
        QScrollArea *_drivePreferencesScrollArea{nullptr};
        QWidget *_noDrivePagewidget{nullptr};
        QStackedWidget *_errorsStackedWidget{nullptr};
        QLabel *_defaultTextLabel{nullptr};

        void initUI();
        QByteArray contents(const QString &path);
        void reset();

        QString getAppErrorText(QString fctCode, ExitCode exitCode, ExitCause exitCause) const;
        QString getSyncPalErrorText(QString fctCode, ExitCode exitCode, ExitCause exitCause, bool userIsAdmin) const;
        QString getConflictText(ConflictType conflictType, ConflictTypeResolution resolution) const;
        QString getInconsistencyText(InconsistencyType inconsistencyType) const;
        QString getCancelText(CancelType cancelType, const QString &path, const QString &destinationPath = "") const;
        QString getErrorMessage(const ErrorInfo &errorInfo) const;
        QString getBackErrorText(const ErrorInfo &errorInfo) const;
        QString getSyncPalSystemErrorText(const QString &err, ExitCause exitCause) const;
        QString getSyncPalBackErrorText(const QString &err, ExitCause exitCause, bool userIsAdmin) const;
        QString getErrorLevelNodeText(const ErrorInfo &errorInfo) const;

        void createErrorTabWidgetIfNeeded(int driveDbId);
        void refreshErrorList(int driveDbId);

    private slots:
        void onExit();
        void onPreferencesButtonClicked();
        void onOpenHelp();
        void onDriveSelected(int id);
        void onAddDrive();
        void onRemoveDrive(int driveDbId);
        void onDisplayDriveErrors(int driveDbId);
        void onDisplayGeneralErrors();
        void onDisplayDriveParameters();
        void onDisplayPreferences();
        void onBackButtonClicked();
        void onSetStyle(bool darkTheme);
        void onOpenFolder(const QString &filePath);
        void onDebugReporterDone(bool retCode, const QString &debugId = QString());
        void retranslateUi();
        void onPauseSync(int syncDbId = 0);
        void onResumeSync(int syncDbId = 0);
        void onRunSync(int syncDbId = 0);
        void onClearErrors(int syncDbId, bool autoResolved);
};

}  // namespace KDC
