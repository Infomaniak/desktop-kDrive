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

#include "adddrivewizard.h"
#include "custommessagebox.h"
#include "enablestateholder.h"
#include "clientgui.h"
#include "guirequests.h"
#include "libcommon/theme/theme.h"
#include "libcommon/utility/utility.h"
#include "libcommongui/utility/utility.h"
#include "libcommongui/matomoclient.h"

#include <QBoxLayout>
#include <QDir>
#include <QLoggingCategory>

namespace KDC {

static const QSize windowSize(625, 800);
static const int boxHMargin = 40;
static const int boxVTMargin = 20;
static const int boxVBMargin = 40;

Q_LOGGING_CATEGORY(lcAddDriveWizard, "gui.adddrivewizard", QtInfoMsg)

AddDriveWizard::AddDriveWizard(std::shared_ptr<ClientGui> gui, int userDbId, QWidget *parent) :
    CustomDialog(false, parent),
    _gui(gui),
    _currentStep(userDbId ? Login : None),
    _userDbId(userDbId),
    _action(KDC::GuiUtility::WizardAction::OpenFolder) {
    initUI();
    start();
}

void AddDriveWizard::setButtonIcon(const QColor &value) {
    if (_addDriveLiteSyncWidget) {
        _addDriveLiteSyncWidget->setButtonIcon(value);
    }
    if (_addDriveServerFoldersWidget) {
        _addDriveServerFoldersWidget->setButtonIcon(value);
    }
    if (_addDriveLocalFolderWidget) {
        _addDriveLocalFolderWidget->setButtonIcon(value);
    }
}

void AddDriveWizard::initUI() {
    setMinimumSize(windowSize);
    setMaximumSize(windowSize);

    QVBoxLayout *mainLayout = this->mainLayout();
    mainLayout->setContentsMargins(boxHMargin, boxVTMargin, boxHMargin, boxVBMargin);

    _stepStackedWidget = new QStackedWidget(this);
    mainLayout->addWidget(_stepStackedWidget);

    _addDriveLoginWidget = new AddDriveLoginWidget(this);
    _stepStackedWidget->insertWidget(Login, _addDriveLoginWidget);

    _addDriveListWidget = new AddDriveListWidget(_gui, this);
    _stepStackedWidget->insertWidget(ListDrives, _addDriveListWidget);

    _addDriveLiteSyncWidget = new AddDriveLiteSyncWidget(this);
    _stepStackedWidget->insertWidget(RemoteFolders, _addDriveLiteSyncWidget);

    _addDriveServerFoldersWidget = new AddDriveServerFoldersWidget(_gui, this);
    _stepStackedWidget->insertWidget(RemoteFolders, _addDriveServerFoldersWidget);

    _addDriveLocalFolderWidget = new AddDriveLocalFolderWidget(_gui, this);
    _stepStackedWidget->insertWidget(LocalFolder, _addDriveLocalFolderWidget);

    _addDriveExtensionSetupWidget = new AddDriveExtensionSetupWidget(this);
    _stepStackedWidget->insertWidget(Confirmation, _addDriveExtensionSetupWidget);

    _addDriveConfirmationWidget = new AddDriveConfirmationWidget(this);
    _stepStackedWidget->insertWidget(Confirmation, _addDriveConfirmationWidget);

    connect(_addDriveLoginWidget, &AddDriveLoginWidget::terminated, this, &AddDriveWizard::onStepTerminated);
    connect(_addDriveListWidget, &AddDriveListWidget::terminated, this, &AddDriveWizard::onStepTerminated);
    connect(_addDriveLiteSyncWidget, &AddDriveLiteSyncWidget::terminated, this, &AddDriveWizard::onStepTerminated);
    connect(_addDriveServerFoldersWidget, &AddDriveServerFoldersWidget::terminated, this, &AddDriveWizard::onStepTerminated);
    connect(_addDriveLocalFolderWidget, &AddDriveLocalFolderWidget::terminated, this, &AddDriveWizard::onStepTerminated);
    connect(_addDriveExtensionSetupWidget, &AddDriveExtensionSetupWidget::terminated, this, &AddDriveWizard::onStepTerminated);
    connect(_addDriveConfirmationWidget, &AddDriveConfirmationWidget::terminated, this, &AddDriveWizard::onStepTerminated);

    connect(this, &CustomDialog::exit, this, &AddDriveWizard::onExit);
}

void AddDriveWizard::start() {
    startNextStep();
}

void AddDriveWizard::startNextStep(bool backward) {
    // Determine next step
    bool setupDrive = false;

    _currentStep = (Step) (_currentStep + (backward ? -1 : 1));

    if (_currentStep == Login && backward) {
        if (!_addDriveListWidget->isAddUserClicked()) {
            exit();
        } else {
            _addDriveListWidget->setAddUserClicked(false);
        }
    } else if (_currentStep == LiteSync) {
        VirtualFileMode virtualFileMode;
        ExitCode exitCode = GuiRequests::bestAvailableVfsMode(virtualFileMode);
        if (exitCode != ExitCode::Ok) {
            qCWarning(lcAddDriveWizard()) << "Error in Requests::bestAvailableVfsMode";
            return;
        }

        if (virtualFileMode != VirtualFileMode::Win && virtualFileMode != VirtualFileMode::Mac) {
            // Skip Lite Sync step
            _currentStep = (Step) (_currentStep + (backward ? -1 : 1));
        }
    } else if (_currentStep == RemoteFolders && _liteSync) {
        // Skip Remote Folders step
        _currentStep = (Step) (_currentStep + (backward ? -1 : 1));
    } else if (_currentStep == ExtensionSetup) {
        setupDrive = true;

        VirtualFileMode virtualFileMode;
        ExitCode exitCode = GuiRequests::bestAvailableVfsMode(virtualFileMode);
        if (exitCode != ExitCode::Ok) {
            qCWarning(lcAddDriveWizard) << "Error in Requests::bestAvailableVfsMode";
            return;
        }

        bool skipExtSetup = true;
#ifdef Q_OS_MAC
        if (virtualFileMode == VirtualFileMode::Mac && _liteSync) {
            // Check LiteSync ext authorizations
            std::string liteSyncExtErrorDescr;
            bool liteSyncExtOk = CommonUtility::isLiteSyncExtEnabled() &&
                                 CommonUtility::isLiteSyncExtFullDiskAccessAuthOk(liteSyncExtErrorDescr);
            if (!liteSyncExtErrorDescr.empty()) {
                qCWarning(lcAddDriveWizard) << "Error in CommonUtility::isLiteSyncExtFullDiskAccessAuthOk: "
                                            << liteSyncExtErrorDescr.c_str();
            }
            skipExtSetup = liteSyncExtOk;
        }
#endif

        if (skipExtSetup) {
            // Skip Extension Setup step
            _currentStep = (Step) (_currentStep + (backward ? -1 : 1));
        }
    }

    // Init step
    _stepStackedWidget->setCurrentIndex(_currentStep);

    if (_currentStep == Login) {
        setBackgroundForcedColor(Qt::white);
        _addDriveLoginWidget->init();
    } else if (_currentStep == ListDrives) {
        setBackgroundForcedColor(QColor());
        _addDriveListWidget->setUserDbId(_userDbId);
        _addDriveListWidget->setDrivesData();
        _addDriveListWidget->setUsersData();
    } else if (_currentStep == LiteSync) {
        setBackgroundForcedColor(QColor());
    } else if (_currentStep == RemoteFolders) {
        setBackgroundForcedColor(QColor());
        _addDriveServerFoldersWidget->init(_userDbId, _driveInfo);
    } else if (_currentStep == LocalFolder) {
        setBackgroundForcedColor(QColor());
        _addDriveLocalFolderWidget->setDrive(_driveInfo.name());
        _addDriveLocalFolderWidget->setLiteSync(_liteSync);
        QString localFolderPath = QString::fromStdString(Theme::instance()->appName());
        if (!QDir(localFolderPath).isAbsolute()) {
            localFolderPath = QDir::homePath() + dirSeparator + localFolderPath;
        }
        QString goodLocalFolderPath;
        QString error;
        ExitCode exitCode = GuiRequests::findGoodPathForNewSync(_syncDbId, localFolderPath, goodLocalFolderPath, error);
        if (exitCode != ExitCode::Ok) {
            qCWarning(lcAddDriveWizard()) << "Error in Requests::findGoodPathForNewSyncFolder : " << error;
            goodLocalFolderPath = localFolderPath;
        }

        _addDriveLocalFolderWidget->setLocalFolderPath(goodLocalFolderPath);
    } else if (_currentStep == ExtensionSetup) {
        setBackgroundForcedColor(QColor());
    } else if (_currentStep == Confirmation) {
        setBackgroundForcedColor(QColor());
        _addDriveConfirmationWidget->setFolderPath(_localFolderPath);
    }

    if (setupDrive && !addSync(_userDbId, _driveInfo.accountId(), _driveInfo.driveId(), _localFolderPath, _serverFolderPath,
                               _liteSync, _blackList, _whiteList)) {
        qCWarning(lcAddDriveWizard()) << "Error in addSync!";
        reject();
    }
}

bool AddDriveWizard::addSync(int userDbId, int accountId, int driveId, const QString &localFolderPath,
                             const QString &serverFolderPath, bool liteSync, const QSet<QString> &blackList,
                             const QSet<QString> &whiteList) {
    QString localFolderPathNormalized = QDir::fromNativeSeparators(localFolderPath);

    const QDir localFolderDir(localFolderPathNormalized);
    if (localFolderDir.exists()) {
        KDC::CommonGuiUtility::setupFavLink(localFolderPathNormalized);
    } else {
        if (localFolderDir.mkpath(localFolderPathNormalized)) {
            KDC::CommonGuiUtility::setupFavLink(localFolderPathNormalized);
        } else {
            qCWarning(lcAddDriveWizard()) << "Failed to create local folder" << localFolderDir.path();
            CustomMessageBox msgBox(QMessageBox::Warning, tr("Failed to create local folder %1").arg(localFolderDir.path()),
                                    QMessageBox::Ok, this);
            msgBox.exec();
            return false;
        }
    }

    ExitCode exitCode = GuiRequests::addSync(userDbId, accountId, driveId, localFolderPathNormalized, serverFolderPath, QString(),
                                             liteSync, blackList, whiteList, _syncDbId);
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcAddDriveWizard()) << "Error in Requests::addSync";
        CustomMessageBox msgBox(QMessageBox::Warning, tr("Failed to create new synchronization"), QMessageBox::Ok, this);
        msgBox.exec();
        return false;
    }

    return true;
}

void AddDriveWizard::onStepTerminated(bool next) {
    if (_currentStep == Login) {
        if (next) {
            _userDbId = _addDriveLoginWidget->userDbId();
        } else {
            _userDbId = 0;
        }
        startNextStep(!next);
    } else if (_currentStep == ListDrives) {
        MatomoClient::sendVisit(MatomoNameField::PG_AddNewDrive_SelectDrive);
        _driveInfo = _addDriveListWidget->driveInfo();
        _userDbId = _driveInfo.userDbId();
        bool found = false;
        bool stayCurrentStep = false;
        if (next) {
            for (auto &sync: _gui->syncInfoMap()) {
                int driveId = 0;
                GuiRequests::getDriveIdFromSyncDbId(sync.first, driveId);
                if (driveId == _driveInfo.driveId()) {
                    found = true;
                    break;
                }
            }
            if (found) {
                CustomMessageBox msgBox(
                        QMessageBox::Warning,
                        tr("The kDrive %1 is already synchronized on this computer. Continue anyway?").arg(_driveInfo.name()),
                        QMessageBox::Yes | QMessageBox::No, this);
                if (msgBox.execAndMoveToCenter(KDC::GuiUtility::getTopLevelWidget(this)) != QMessageBox::Yes) {
                    stayCurrentStep = true;
                }
            }
        }

        if (!stayCurrentStep) {
            startNextStep(!next);
        }
    } else if (_currentStep == LiteSync) {
        MatomoClient::sendVisit(MatomoNameField::PG_AddNewDrive_ActivateLiteSync);
        if (next) {
            _liteSync = _addDriveLiteSyncWidget->liteSync();
        }
        startNextStep(!next);
    } else if (_currentStep == RemoteFolders) {
        MatomoClient::sendVisit(MatomoNameField::PG_AddNewDrive_SelectRemoteFolder);
        if (next) {
            _blackList = _addDriveServerFoldersWidget->createBlackList();
            if (!GuiUtility::checkBlacklistSize(_blackList.size(), this)) return;
            _whiteList = _addDriveServerFoldersWidget->createWhiteList();
        }
        startNextStep(!next);
    } else if (_currentStep == LocalFolder) {
        MatomoClient::sendVisit(MatomoNameField::PG_AddNewDrive_SelectLocalFolder);
        if (next) {
            _localFolderPath = _addDriveLocalFolderWidget->localFolderPath();
            if (_liteSync) {
                _liteSync = _addDriveLocalFolderWidget->folderCompatibleWithLiteSync();
            }
        }
        startNextStep(!next);
    } else if (_currentStep == ExtensionSetup) {
        MatomoClient::sendVisit(MatomoNameField::PG_AddNewDrive_ExtensionSetup);
        startNextStep(!next);
    } else if (_currentStep == Confirmation) {
        MatomoClient::sendVisit(MatomoNameField::PG_AddNewDrive_Confirmation);
        _action = _addDriveConfirmationWidget->action();
        accept();
    }
}

void AddDriveWizard::onExit() {
    reject();
}

} // namespace KDC
