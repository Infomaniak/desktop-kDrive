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

#pragma once

#include "customdialog.h"
#include "adddriveloginwidget.h"
#include "adddriveslitesyncwidget.h"
#include "adddriveserverfolderswidget.h"
#include "adddrivelocalfolderwidget.h"
#include "adddriveextensionsetupwidget.h"
#include "adddriveconfirmationwidget.h"
#include "adddrivelistwidget.h"
#include "libcommon/info/userinfo.h"
#include "libcommon/info/accountinfo.h"
#include "libcommon/info/driveinfo.h"
#include "libcommon/info/syncinfo.h"

#include <QStackedWidget>

namespace KDC {

class ClientGui;

class AddDriveWizard : public CustomDialog {
        Q_OBJECT

    public:
        explicit AddDriveWizard(std::shared_ptr<ClientGui> gui, int userDbId, QWidget *parent = nullptr);

        inline KDC::GuiUtility::WizardAction nextAction() { return _action; }
        inline QString localFolderPath() { return _localFolderPath; }
        inline int syncDbId() { return _syncDbId; }

    private:
        enum Step {
            None = -1,
            Login,
            ListDrives,
            LiteSync,
            RemoteFolders,
            LocalFolder,
            ExtensionSetup,
            Confirmation
        };

        std::shared_ptr<ClientGui> _gui;

        QStackedWidget *_stepStackedWidget{nullptr};
        AddDriveLoginWidget *_addDriveLoginWidget{nullptr};
        AddDriveListWidget *_addDriveListWidget{nullptr};
        AddDriveLiteSyncWidget *_addDriveLiteSyncWidget{nullptr};
        AddDriveServerFoldersWidget *_addDriveServerFoldersWidget{nullptr};
        AddDriveLocalFolderWidget *_addDriveLocalFolderWidget{nullptr};
        AddDriveExtensionSetupWidget *_addDriveExtensionSetupWidget{nullptr};
        AddDriveConfirmationWidget *_addDriveConfirmationWidget{nullptr};
        Step _currentStep;
        QString _loginUrl;
        bool _liteSync{false};
        QString _serverFolderPath;
        QSet<QString> _blackList;
        QSet<QString> _whiteList;
        QString _localFolderPath;
        int _userDbId;
        DriveAvailableInfo _driveInfo;
        int _syncDbId{0};
        KDC::GuiUtility::WizardAction _action;

        void setButtonIcon(const QColor &value) override;

        void initUI();
        void start();
        void startNextStep(bool backward = false);
        bool addSync(int userDbId, int accountId, int driveId, const QString &localFolderPath, const QString &serverFolderPath,
                     bool liteSync, const QSet<QString> &blackList, const QSet<QString> &whiteList);

    private slots:
        void onStepTerminated(bool next = true);
        void onExit();
};

} // namespace KDC
