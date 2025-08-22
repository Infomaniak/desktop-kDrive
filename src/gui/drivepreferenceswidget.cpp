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

#include "drivepreferenceswidget.h"
#include "localfolderdialog.h"
#include "serverbasefolderdialog.h"
#include "serverfoldersdialog.h"
#include "confirmsynchronizationdialog.h"
#include "bigfoldersdialog.h"
#include "custommessagebox.h"
#include "custompushbutton.h"

#ifdef Q_OS_MAC
#include "extensionsetupdialog.h"
#endif

#include "guiutility.h"
#include "languagechangefilter.h"
#include "enablestateholder.h"
#include "guirequests.h"
#include "clientgui.h"
#include "libcommongui/matomoclient.h"
#include "libcommon/utility/utility.h"
#include "libcommongui/utility/utility.h"
#include "libcommon/utility/qlogiffail.h"
#include "libcommonserver/io/iohelper.h"

#include <QDesktopServices>
#include <QDir>
#include <QIcon>
#include <QLabel>
#include <QLayout>
#include <QStandardPaths>
#include <QStyle>
#include <QMutexLocker>

namespace KDC {

static const int boxHMargin = 20;
static const int boxVMargin = 20;
static const int boxVSpacing = 12;
static const int textHSpacing = 5;
static const int avatarSize = 40;

static const QString folderBlocName("folderBloc");

Q_LOGGING_CATEGORY(lcDrivePreferencesWidget, "gui.drivepreferenceswidget", QtInfoMsg)

DrivePreferencesWidget::DrivePreferencesWidget(std::shared_ptr<ClientGui> gui, QWidget *parent) :
    LargeWidgetWithCustomToolTip(parent),
    _gui(gui) {
    setContentsMargins(0, 0, 0, 0);

    /*
     *  _mainVBox
     *      _displayErrorsWidget
     *      _displayInfosWidget
     *      _displayBigFoldersWarningWidget
     *      foldersHeaderHBox
     *          foldersLabel
     *          addFolderButton
     *      folderBloc[]
     *      synchronizationLabel
     *      synchronizationBloc
     *          liteSyncBox
     *              liteSync1HBox
     *                  liteSyncLabel
     *                  _liteSyncSwitch
     *              _liteSyncDescriptionLabel
     *          driveFoldersWidget
     *              driveFoldersVBox
     *                  driveFoldersLabel
     *                  driveFoldersStatusLabel
     *      locationLabel
     *      locationBloc
     *          _locationWidget
     *              _locationLayout
     *      notificationsLabel
     *      notificationsBloc
     *          notificationsBox
     *              notifications1HBox
     *                  notificationsTitleLabel
     *                  _notificationsSwitch
     *              notifications2HBox
     *                  notificationsDescriptionLabel
     *      connectedWithLabel
     *      connectedWithBloc
     *          connectedWithBox
     *              _accountAvatarLabel
     *              connectedWithVBox
     *                  _accountNameLabel
     *                  _accountMailLabel
     */

    _mainVBox = new QVBoxLayout();
    _mainVBox->setContentsMargins(boxHMargin, boxVMargin, boxHMargin, boxVMargin);
    _mainVBox->setSpacing(boxVSpacing);
    setLayout(_mainVBox);

    //
    // Synchronization errors
    //
    _displayErrorsWidget =
            new ActionWidget(":/client/resources/icons/actions/warning.svg", tr("Synchronization errors and information."), this);
    _displayErrorsWidget->setObjectName("displayErrorsWidget");
    _mainVBox->addWidget(_displayErrorsWidget);
    _displayErrorsWidget->setVisible(false);

    //
    // Big folders warning
    //
    _displayBigFoldersWarningWidget =
            new ActionWidget(":/client/resources/icons/actions/warning.svg",
                             tr("Some folders were not synchronized because they are too large."), this);
    _displayBigFoldersWarningWidget->setObjectName("displayBigFoldersWarningWidget");
    _displayBigFoldersWarningWidget->setVisible(false);
    _mainVBox->addWidget(_displayBigFoldersWarningWidget);

    //
    //  Search bloc
    //
    initializeSearchBloc();

    //
    // Folders blocs
    //
    QHBoxLayout *foldersHeaderHBox = new QHBoxLayout();
    foldersHeaderHBox->setContentsMargins(0, 0, 0, 0);
    _mainVBox->addLayout(foldersHeaderHBox);
    _foldersBeginIndex = _mainVBox->indexOf(foldersHeaderHBox) + 1;

    _foldersLabel = new QLabel(this);
    _foldersLabel->setObjectName("blocLabel");
    foldersHeaderHBox->addWidget(_foldersLabel);
    foldersHeaderHBox->addStretch();

    _addLocalFolderButton =
            new CustomPushButton(":/client/resources/icons/actions/add.svg", tr("Synchronize a local folder"), this);
    _addLocalFolderButton->setObjectName("addFolderButton");
    foldersHeaderHBox->addWidget(_addLocalFolderButton);

    //
    // Notifications bloc
    //
    _notificationsLabel = new QLabel(this);
    _notificationsLabel->setObjectName("blocLabel");
    _mainVBox->addWidget(_notificationsLabel);

    PreferencesBlocWidget *notificationsBloc = new PreferencesBlocWidget(this);
    _mainVBox->addWidget(notificationsBloc);

    QBoxLayout *notificationsBox = notificationsBloc->addLayout(QBoxLayout::Direction::TopToBottom);

    QHBoxLayout *notifications1HBox = new QHBoxLayout();
    notifications1HBox->setContentsMargins(0, 0, 0, 0);
    notifications1HBox->setSpacing(0);
    notificationsBox->addLayout(notifications1HBox);

    _notificationsTitleLabel = new QLabel(this);
    notifications1HBox->addWidget(_notificationsTitleLabel);

    _notificationsSwitch = new CustomSwitch(this);
    _notificationsSwitch->setLayoutDirection(Qt::RightToLeft);
    _notificationsSwitch->setAttribute(Qt::WA_MacShowFocusRect, false);
    notifications1HBox->addWidget(_notificationsSwitch);

    QHBoxLayout *notifications2HBox = new QHBoxLayout();
    notifications2HBox->setContentsMargins(0, 0, 0, 0);
    notifications2HBox->setSpacing(0);
    notificationsBox->addLayout(notifications2HBox);

    _notificationsDescriptionLabel = new QLabel(this);
    _notificationsDescriptionLabel->setObjectName("description");
    _notificationsDescriptionLabel->setWordWrap(true);
    notifications2HBox->addWidget(_notificationsDescriptionLabel);

    //
    // Connected with bloc
    //
    _connectedWithLabel = new QLabel(this);
    _connectedWithLabel->setObjectName("blocLabel");
    _mainVBox->addWidget(_connectedWithLabel);

    PreferencesBlocWidget *connectedWithBloc = new PreferencesBlocWidget(this);
    _mainVBox->addWidget(connectedWithBloc);

    QBoxLayout *connectedWithBox = connectedWithBloc->addLayout(QBoxLayout::Direction::LeftToRight);

    _userAvatarLabel = new QLabel(this);
    _userAvatarLabel->setObjectName("accountAvatarLabel");
    connectedWithBox->addWidget(_userAvatarLabel);

    QVBoxLayout *connectedWithVBox = new QVBoxLayout();
    connectedWithVBox->setContentsMargins(0, 0, 0, 0);
    connectedWithVBox->setSpacing(textHSpacing);
    connectedWithBox->addLayout(connectedWithVBox);

    _userNameLabel = new QLabel(this);
    _userNameLabel->setObjectName("accountNameLabel");
    connectedWithVBox->addWidget(_userNameLabel);

    _userMailLabel = new QLabel(this);
    _userMailLabel->setObjectName("accountMailLabel");
    connectedWithVBox->addWidget(_userMailLabel);

    connectedWithBox->addStretch();

    _removeDriveButton = new CustomToolButton(this);
    _removeDriveButton->setIconPath(":/client/resources/icons/actions/error-sync.svg");
    connectedWithBox->addWidget(_removeDriveButton);

    _mainVBox->addStretch();

    // Init labels and setup connection for on the fly translation
    retranslateUi();
    LanguageChangeFilter *languageFilter = new LanguageChangeFilter(this);
    installEventFilter(languageFilter);
    connect(languageFilter, &LanguageChangeFilter::retranslate, this, &DrivePreferencesWidget::retranslateUi);

    connect(_displayErrorsWidget, &ActionWidget::clicked, this, &DrivePreferencesWidget::onErrorsWidgetClicked);
    connect(_displayBigFoldersWarningWidget, &ActionWidget::clicked, this,
            &DrivePreferencesWidget::onBigFoldersWarningWidgetClicked);
    connect(_addLocalFolderButton, &CustomPushButton::clicked, this, &DrivePreferencesWidget::onAddLocalFolder);
    connect(_notificationsSwitch, &CustomSwitch::clicked, this, &DrivePreferencesWidget::onNotificationsSwitchClicked);
    connect(this, &DrivePreferencesWidget::errorAdded, this, &DrivePreferencesWidget::onErrorAdded);
    connect(_removeDriveButton, &CustomToolButton::clicked, this, &DrivePreferencesWidget::onRemoveDrive);
    connect(this, &DrivePreferencesWidget::newBigFolderDiscovered, this, &DrivePreferencesWidget::onNewBigFolderDiscovered);
    connect(this, &DrivePreferencesWidget::undecidedListsCleared, this, &DrivePreferencesWidget::onUndecidedListsCleared);

    connect(_gui.get(), &ClientGui::vfsConversionCompleted, this, &DrivePreferencesWidget::onVfsConversionCompleted);
    connect(_gui.get(), &ClientGui::driveBeingRemoved, this, &DrivePreferencesWidget::onDriveBeingRemoved);
}

void DrivePreferencesWidget::setDrive(int driveDbId, bool unresolvedErrors) {
    const auto &driveInfoMapIt = _gui->driveInfoMap().find(driveDbId);
    if (driveInfoMapIt == _gui->driveInfoMap().end()) {
        qCWarning(lcDrivePreferencesWidget()) << "Drive not found in driveInfoMap for driveDbId=" << _driveDbId;
        return;
    }

    if (_driveDbId != driveDbId) {
        reset();
    }

    _driveDbId = driveDbId;
    _displayErrorsWidget->setVisible(unresolvedErrors);
    _displayBigFoldersWarningWidget->setVisible(existUndecidedSet());
    _notificationsSwitch->setChecked(driveInfoMapIt->second.notifications());
    _notificationsSwitch->setVisible(true);

    updateUserInfo();
    updateFoldersBlocs();
}

void DrivePreferencesWidget::reset() {
    _driveDbId = 0;
    _displayErrorsWidget->setVisible(false);
    _displayBigFoldersWarningWidget->setVisible(false);
    resetFoldersBlocs();
    _notificationsSwitch->setChecked(true);
    _userAvatarLabel->setPixmap(QPixmap());
    _userNameLabel->setText(QString());
    _userMailLabel->setText(QString());
}

void DrivePreferencesWidget::showErrorBanner(bool unresolvedErrors) {
    _displayErrorsWidget->setVisible(unresolvedErrors);
}

void DrivePreferencesWidget::refreshStatus() {
    if (!_driveDbId) return;

    if (const auto driveInfoMapIt = _gui->driveInfoMap().find(_driveDbId); driveInfoMapIt != _gui->driveInfoMap().cend()) {
        const auto &driveInfo = driveInfoMapIt->second;
        if (!driveInfo.isBeingDeleted()) {
            // Re-enable the drive preferences widget after a failed deletion attempt.
            setCustomToolTipText("");
            GuiUtility::setEnabledRecursively(this, true);
        }
    }

    const QList<PreferencesBlocWidget *> folderBlocList = findChildren<PreferencesBlocWidget *>(folderBlocName);
    for (PreferencesBlocWidget *folderBloc: folderBlocList) {
        folderBloc->updateBloc();
    }

    // Hack to trigger a repaint of the _mMainVBox children.
    _notificationsLabel->setVisible(false);
    _notificationsLabel->setVisible(true);
}

void DrivePreferencesWidget::showEvent(QShowEvent *event) {
    _gui->activateLoadInfo();
    QList<PreferencesBlocWidget *> folderBlocList = findChildren<PreferencesBlocWidget *>(folderBlocName);
    for (PreferencesBlocWidget *folderBloc: folderBlocList) {
        if (auto folderItemWidget = folderBloc->findChild<FolderItemWidget *>(); folderItemWidget != nullptr) {
            folderItemWidget->closeFolderView();
        }
    }
    LargeWidgetWithCustomToolTip::showEvent(event);
}

void DrivePreferencesWidget::onVfsConversionCompleted(int syncDbId) {
    QList<PreferencesBlocWidget *> folderBlocList = findChildren<PreferencesBlocWidget *>(folderBlocName);
    for (PreferencesBlocWidget *folderBloc: folderBlocList) {
        FolderItemWidget *folderItemWidget = folderBloc->findChild<FolderItemWidget *>();
        if (folderItemWidget && folderItemWidget->syncDbId() == syncDbId) {
            auto syncInfoMapIt = _gui->syncInfoMap().find(folderItemWidget->syncDbId());
            if (syncInfoMapIt != _gui->syncInfoMap().end()) {
                folderItemWidget->setLiteSyncActivated(syncInfoMapIt->second.virtualFileMode() != VirtualFileMode::Off);
            }
        }
    }
}

bool DrivePreferencesWidget::existUndecidedSet() {
    bool ret = false;
    if (_driveDbId) {
        for (const auto &syncInfoMapElt: _gui->syncInfoMap()) {
            if (syncInfoMapElt.second.driveDbId() == _driveDbId) {
                if (syncInfoMapElt.second.status() == SyncStatus::Undefined) {
                    continue;
                }

                if (syncInfoMapElt.second.virtualFileMode() == VirtualFileMode::Win ||
                    syncInfoMapElt.second.virtualFileMode() == VirtualFileMode::Mac) {
                    continue;
                }

                QSet<QString> undecidedSet;
                ExitCode exitCode = GuiRequests::getSyncIdSet(syncInfoMapElt.first, SyncNodeType::UndecidedList, undecidedSet);
                if (exitCode != ExitCode::Ok) {
                    qCWarning(lcDrivePreferencesWidget()) << "Error in Requests::getSyncIdSet";
                    break;
                }

                if (!undecidedSet.empty()) {
                    ret = true;
                    break;
                }
            }
        }
    }

    return ret;
}

void DrivePreferencesWidget::updateUserInfo() {
    const auto &driveInfoMapIt = _gui->driveInfoMap().find(_driveDbId);
    if (driveInfoMapIt == _gui->driveInfoMap().end()) {
        qCWarning(lcDrivePreferencesWidget()) << "Drive not found in driveInfoMap for driveDbId=" << _driveDbId;
        return;
    }

    const auto &accountInfoMapIt = _gui->accountInfoMap().find(driveInfoMapIt->second.accountDbId());
    if (accountInfoMapIt == _gui->accountInfoMap().end()) {
        qCWarning(lcDrivePreferencesWidget())
                << "Account not found in accountInfoMap for accountDbId=" << driveInfoMapIt->second.accountDbId();
        return;
    }

    _userDbId = accountInfoMapIt->second.userDbId();

    const auto userInfoMapIt = _gui->userInfoMap().find(accountInfoMapIt->second.userDbId());
    if (userInfoMapIt == _gui->userInfoMap().end()) {
        qCWarning(lcDrivePreferencesWidget())
                << "User not found in userInfoMap for userDbId=" << accountInfoMapIt->second.userDbId();
        return;
    }

    if (!userInfoMapIt->second.avatar().isNull()) {
        _userAvatarLabel->setPixmap(KDC::GuiUtility::getAvatarFromImage(userInfoMapIt->second.avatar())
                                            .scaled(avatarSize, avatarSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    _userNameLabel->setText(userInfoMapIt->second.name());
    _userMailLabel->setText(userInfoMapIt->second.email());
}

void DrivePreferencesWidget::askEnableLiteSync(const std::function<void(bool)> &callback) {
    VirtualFileMode virtualFileMode;
    ExitCode exitCode = GuiRequests::bestAvailableVfsMode(virtualFileMode);
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcDrivePreferencesWidget()) << "Error in Requests::bestAvailableVfsMode";
        return;
    }

    if (virtualFileMode == VirtualFileMode::Win || virtualFileMode == VirtualFileMode::Mac ||
        virtualFileMode == VirtualFileMode::Suffix) {
        CustomMessageBox msgBox(QMessageBox::Question, tr("Do you really want to turn on Lite Sync?"),
                                tr("This operation may take from a few seconds to a few minutes depending on the size of the "
                                   "folder."),
                                false, QMessageBox::NoButton, this);
        msgBox.addButton(tr("CONFIRM"), QMessageBox::Yes);
        msgBox.addButton(tr("CANCEL"), QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::Yes);
        int result = msgBox.exec();
        callback(result == QMessageBox::Yes);
    }
}

void DrivePreferencesWidget::askDisableLiteSync(const std::function<void(bool, bool)> &callback, int syncDbId) {
    // Available space
    qint64 freeSize = KDC::CommonUtility::freeDiskSpace(dirSeparator);

    // Compute folders size sum
    qint64 localFoldersSize = 0;
    qint64 localFoldersDiskSize = 0;
    auto syncInfoIt = _gui->syncInfoMap().find(syncDbId);
    if (syncInfoIt != _gui->syncInfoMap().end()) {
        localFoldersSize += KDC::GuiUtility::folderSize(syncInfoIt->second.localPath());
        localFoldersDiskSize += KDC::GuiUtility::folderDiskSize(syncInfoIt->second.localPath());
    }

    qint64 diskSpaceMissing = localFoldersSize - localFoldersDiskSize - freeSize;
    bool diskSpaceWarning = diskSpaceMissing > 0;

    CustomMessageBox msgBox(QMessageBox::Question, tr("Do you really want to turn off Lite Sync?"),
                            diskSpaceWarning
                                    ? tr("You don't have enough space to sync all the files on your kDrive (%1 missing)."
                                         " If you turn off Lite Sync, you need to select which folders to sync on your computer."
                                         " In the meantime, the synchronization of your kDrive will be paused.")
                                              .arg(KDC::CommonGuiUtility::octetsToString(diskSpaceMissing))
                                    : tr("If you turn off Lite Sync, all files will be downloaded to your computer."),
                            diskSpaceWarning, QMessageBox::NoButton, this);
    msgBox.addButton(tr("CONFIRM"), QMessageBox::Yes);
    msgBox.addButton(tr("CANCEL"), QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::Yes);
    int result = msgBox.exec();
    callback(result == QMessageBox::Yes, diskSpaceWarning);
}

bool DrivePreferencesWidget::switchVfsOn(int syncDbId) {
    // Change the folder vfs mode and load the plugin
    ExitCode exitCode = GuiRequests::setSupportsVirtualFiles(syncDbId, true);
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcDrivePreferencesWidget()) << "Error in Requests::setSupportsVirtualFiles";
        return false;
    }

#ifdef Q_OS_MAC
    // Setup Vfs extension (Mac)
    VirtualFileMode virtualFileMode;
    exitCode = GuiRequests::bestAvailableVfsMode(virtualFileMode);
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcDrivePreferencesWidget()) << "Error in Requests::bestAvailableVfsMode";
        return false;
    }

    if (virtualFileMode == VirtualFileMode::Mac) {
        // Check LiteSync ext authorizations
        std::string liteSyncExtErrorDescr;
        bool liteSyncExtOk =
                CommonUtility::isLiteSyncExtEnabled() && CommonUtility::isLiteSyncExtFullDiskAccessAuthOk(liteSyncExtErrorDescr);
        if (!liteSyncExtErrorDescr.empty()) {
            qCWarning(lcDrivePreferencesWidget)
                    << "Error in CommonUtility::isLiteSyncExtFullDiskAccessAuthOk: " << liteSyncExtErrorDescr.c_str();
        }
        if (!liteSyncExtOk) {
            // Extension Setup step
            ExtensionSetupDialog dialog(this);
            if (dialog.execAndMoveToCenter(KDC::GuiUtility::getTopLevelWidget(this)) != QDialog::Accepted) {
                // return false;
            }
        }
    }
#endif

    // Setting to Unspecified retains existing data.
    exitCode = GuiRequests::setRootPinState(syncDbId, PinState::Unspecified);
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcDrivePreferencesWidget()) << "Error in Requests::setRootPinState";
        return false;
    }

    return true;
}

bool DrivePreferencesWidget::switchVfsOff(int syncDbId, bool diskSpaceWarning) {
    ExitCode exitCode = GuiRequests::setSupportsVirtualFiles(syncDbId, false);
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcDrivePreferencesWidget()) << "Error in Requests::setSupportsVirtualFiles";
        return false;
    }

    // Wipe pin states
    exitCode = GuiRequests::setRootPinState(syncDbId, PinState::AlwaysLocal);
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcDrivePreferencesWidget()) << "Error in Requests::setRootPinState";
        return false;
    }

    if (diskSpaceWarning) {
        // Pause sync if disk space warning
        exitCode = GuiRequests::syncStop(syncDbId);
        if (exitCode != ExitCode::Ok) {
            qCWarning(lcDrivePreferencesWidget()) << "Error in Requests::syncStop";
            return false;
        }
    }

    return true;
}

void DrivePreferencesWidget::resetFoldersBlocs() {
    QList<PreferencesBlocWidget *> folderBlocList = findChildren<PreferencesBlocWidget *>(folderBlocName);
    for (PreferencesBlocWidget *folderBloc: folderBlocList) {
        folderBloc->deleteLater();
    }

    update();
}

void DrivePreferencesWidget::updateGuardedFoldersBlocs() {
    int foldersNextBeginIndex = _foldersBeginIndex;
    QList<int> syncDbIdList;
    const QList<PreferencesBlocWidget *> folderBlocList = findChildren<PreferencesBlocWidget *>(folderBlocName);
    for (PreferencesBlocWidget *folderBloc: folderBlocList) {
        FolderItemWidget *folderItemWidget = folderBloc->findChild<FolderItemWidget *>();
        if (!folderItemWidget) {
            qCDebug(lcDrivePreferencesWidget) << "Empty folder bloc!";
            continue;
        }
        if (auto syncInfoClient = folderItemWidget->getSyncInfoClient();
            !syncInfoClient || syncInfoClient->driveDbId() != _driveDbId) {
            // Delete bloc when folder doesn't exist anymore or doesn't belong to the current drive
            folderBloc->deleteLater();
        } else {
            // Update folder widget
            folderItemWidget->updateItem();
            syncDbIdList << folderItemWidget->syncDbId();
            const int index = _mainVBox->indexOf(folderBloc) + 1;
            if (foldersNextBeginIndex < index) {
                foldersNextBeginIndex = index;
            }
        }
    }

    for (const auto &syncInfoMapElt: _gui->syncInfoMap()) {
        if (syncInfoMapElt.second.driveDbId() != _driveDbId) {
            continue;
        }

        if (!syncDbIdList.contains(syncInfoMapElt.first)) {
            // Create folder bloc
            PreferencesBlocWidget *folderBloc = new PreferencesBlocWidget(this);
            folderBloc->setObjectName(folderBlocName);
            _mainVBox->insertWidget(foldersNextBeginIndex, folderBloc);

            QBoxLayout *folderBox = folderBloc->addLayout(QBoxLayout::Direction::LeftToRight);

            FolderItemWidget *folderItemWidget = new FolderItemWidget(syncInfoMapElt.first, _gui, this);
            folderItemWidget->setSupportVfs(syncInfoMapElt.second.supportVfs());
            folderItemWidget->setLiteSyncActivated(syncInfoMapElt.second.virtualFileMode() != VirtualFileMode::Off);
            folderBox->addWidget(folderItemWidget);

            QFrame *line = folderBloc->addSeparator();
            line->setVisible(false);

            // Folder tree
            QBoxLayout *folderTreeBox = folderBloc->addLayout(QBoxLayout::Direction::LeftToRight, true);

            FolderTreeItemWidget *folderTreeItemWidget = new FolderTreeItemWidget(_gui, false, this);
            folderTreeItemWidget->setSyncDbId(syncInfoMapElt.first);
            folderTreeItemWidget->setObjectName("updateFolderTreeItemWidget");
            folderTreeItemWidget->setVisible(false);
            folderTreeBox->addWidget(folderTreeItemWidget);

            connect(folderItemWidget, &FolderItemWidget::pauseSync, this, &DrivePreferencesWidget::pauseSync);
            connect(folderItemWidget, &FolderItemWidget::resumeSync, this, &DrivePreferencesWidget::resumeSync);
            connect(folderItemWidget, &FolderItemWidget::unSync, this, &DrivePreferencesWidget::onUnsyncTriggered);
            connect(folderItemWidget, &FolderItemWidget::displayFolderDetail, this,
                    &DrivePreferencesWidget::onDisplayFolderDetail);
            connect(folderItemWidget, &FolderItemWidget::openFolder, this, &DrivePreferencesWidget::onOpenFolder);
            connect(folderItemWidget, &FolderItemWidget::cancelUpdate, this, &DrivePreferencesWidget::onCancelUpdate);
            connect(folderItemWidget, &FolderItemWidget::validateUpdate, this, &DrivePreferencesWidget::onValidateUpdate);
            connect(folderItemWidget, &FolderItemWidget::triggerLiteSyncChanged, this,
                    &DrivePreferencesWidget::onLiteSyncSwitchSyncChanged);
            connect(folderTreeItemWidget, &FolderTreeItemWidget::terminated, this, &DrivePreferencesWidget::onSubfoldersLoaded);
            connect(folderTreeItemWidget, &FolderTreeItemWidget::needToSave, this, &DrivePreferencesWidget::onNeedToSave);
        }
    }
}

void DrivePreferencesWidget::initializeSearchBloc() {
    bool showSearchBloc = !CommonUtility::envVarValue("KDRIVE_SHOW_SEARCH").empty();
    _searchLabel = new QLabel(this);
    _searchLabel->setObjectName("blocLabel");
    _searchLabel->setVisible(showSearchBloc);
    _mainVBox->addWidget(_searchLabel);

    auto *searchBloc = new PreferencesBlocWidget(this);
    searchBloc->setVisible(showSearchBloc);
    _mainVBox->addWidget(searchBloc);

    auto *searchLayout = searchBloc->addLayout(QBoxLayout::Direction::TopToBottom);
    _mainVBox->addLayout(searchLayout);
    auto *searchHLayout = new QHBoxLayout(this);
    _searchBox = new QLineEdit(this);
    searchHLayout->addWidget(_searchBox);
    _searchButton = new QPushButton(this);
    searchHLayout->addWidget(_searchButton);
    searchLayout->addLayout(searchHLayout);
    _searchProgressLabel = new QLabel(this);
    _searchProgressLabel->setVisible(false);
    searchLayout->addWidget(_searchProgressLabel);
    _searchResultView = new QListView(this);
    _searchResultView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _searchResultView->setModel(&_searchResultModel);
    _searchResultView->setVisible(false);
    searchLayout->addWidget(_searchResultView);
    connect(_searchButton, &QPushButton::clicked, this, &DrivePreferencesWidget::onSearch);
    connect(_searchResultView, &QAbstractItemView::doubleClicked, this, &DrivePreferencesWidget::onSearchItemDoubleClicked);
}

void DrivePreferencesWidget::updateFoldersBlocs() {
    if (_updatingFoldersBlocs) {
        return;
    }

    _updatingFoldersBlocs = true;

    updateGuardedFoldersBlocs();

    _updatingFoldersBlocs = false;
}

void DrivePreferencesWidget::refreshFoldersBlocs() const {
    const QList<PreferencesBlocWidget *> folderBlocList = findChildren<PreferencesBlocWidget *>(folderBlocName);
    for (const PreferencesBlocWidget *folderBloc: folderBlocList) {
        folderBloc->refreshFolders();
    }
}

FolderTreeItemWidget *DrivePreferencesWidget::blocTreeItemWidget(PreferencesBlocWidget *folderBloc) {
    if (!folderBloc) {
        return nullptr;
    }

    FolderTreeItemWidget *folderTreeItemWidget = folderBloc->findChild<FolderTreeItemWidget *>();
    if (!folderTreeItemWidget) {
        qCDebug(lcDrivePreferencesWidget) << "Bad folder bloc!";
        return nullptr;
    }

    return folderTreeItemWidget;
}

FolderItemWidget *DrivePreferencesWidget::blocItemWidget(PreferencesBlocWidget *folderBloc) {
    if (!folderBloc) {
        return nullptr;
    }

    FolderItemWidget *folderItemWidget = folderBloc->findChild<FolderItemWidget *>();
    if (!folderItemWidget) {
        qCDebug(lcDrivePreferencesWidget) << "Bad folder bloc!";
        return nullptr;
    }

    return folderItemWidget;
}

QFrame *DrivePreferencesWidget::blocSeparatorFrame(PreferencesBlocWidget *folderBloc) {
    if (!folderBloc) {
        return nullptr;
    }

    QFrame *separatorFrame = folderBloc->findChild<QFrame *>();
    if (!separatorFrame) {
        qCDebug(lcDrivePreferencesWidget) << "Bad folder bloc!";
        return nullptr;
    }

    return separatorFrame;
}

bool DrivePreferencesWidget::addSync(const QString &localFolderPath, bool liteSync, const QString &serverFolderPath,
                                     const QString &serverFolderNodeId, QSet<QString> blackSet, QSet<QString> whiteSet) {
    QString localFolderPathNormalized = QDir::fromNativeSeparators(localFolderPath);

    KDC::CommonGuiUtility::setupFavLink(localFolderPathNormalized);

    int syncDbId;
    ExitCode exitCode = GuiRequests::addSync(_driveDbId, localFolderPathNormalized, serverFolderPath, serverFolderNodeId,
                                             liteSync, blackSet, whiteSet, syncDbId);
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcDrivePreferencesWidget()) << "Error in Requests::addSync";
        CustomMessageBox msgBox(QMessageBox::Warning, tr("Failed to create new synchronization"), QMessageBox::Ok, this);
        msgBox.exec();
        return false;
    }

    return true;
}

bool DrivePreferencesWidget::updateSelectiveSyncList(const QHash<int, QHash<const QString, bool>> &mapUndefinedFolders) {
    bool res = true;
    for (auto it = mapUndefinedFolders.begin(); it != mapUndefinedFolders.end(); ++it) {
        int syncDbId = it.key();
        QSet<QString> undecidedSet;
        ExitCode exitCode = GuiRequests::getSyncIdSet(syncDbId, SyncNodeType::UndecidedList, undecidedSet);
        if (exitCode != ExitCode::Ok) {
            qCWarning(lcDrivePreferencesWidget()) << "Error in Requests::getSyncIdSet";
            res = false;
            continue;
        }

        // If this folder had no undecided entries, skip it.
        if (undecidedSet.isEmpty()) {
            continue;
        }

        // Get blacklisted folders
        QSet<QString> blackSet;
        exitCode = GuiRequests::getSyncIdSet(syncDbId, SyncNodeType::BlackList, blackSet);
        if (exitCode != ExitCode::Ok) {
            qCWarning(lcDrivePreferencesWidget()) << "Error in Requests::getSyncIdSet";
            res = false;
            continue;
        }

        // Remove all the folders that were not selected by the user
        for (auto itUserChoice = it.value().begin(); itUserChoice != it.value().end(); ++itUserChoice) {
            if (!itUserChoice.value()) {
                blackSet << itUserChoice.key();
                undecidedSet.remove(itUserChoice.key());
            }
        }

        // Remove all undecided folders from the blacklist and push them to the whitelist
        QSet<QString> whiteSet;
        foreach (const auto &nodeId, undecidedSet) {
            blackSet.remove(nodeId);
            whiteSet.insert(nodeId);
        }

        // Update the black list
        exitCode = GuiRequests::setSyncIdSet(syncDbId, SyncNodeType::BlackList, blackSet);
        if (exitCode != ExitCode::Ok) {
            qCWarning(lcDrivePreferencesWidget()) << "Error in Requests::setSyncIdSet";
            res = false;
            continue;
        }

        // Clear the undecided list
        exitCode = GuiRequests::setSyncIdSet(syncDbId, SyncNodeType::UndecidedList, QSet<QString>());
        if (exitCode != ExitCode::Ok) {
            qCWarning(lcDrivePreferencesWidget()) << "Error in Requests::setSyncIdSet";
            res = false;
            continue;
        }

        // Update the white list
        exitCode = GuiRequests::setSyncIdSet(syncDbId, SyncNodeType::WhiteList, whiteSet);
        if (exitCode != ExitCode::Ok) {
            qCWarning(lcDrivePreferencesWidget()) << "Error in Requests::setSyncIdSet";
            res = false;
            continue;
        }

        GuiRequests::propagateSyncListChange(syncDbId, !whiteSet.empty());
    }

    return res;
}

/*void DrivePreferencesWidget::onLinkActivated(const QString &link)
{
    if (link == KDC::GuiUtility::learnMoreLink) {
        // Learn more: Lite Sync
        if (!QDesktopServices::openUrl(QUrl(LEARNMORE_LITESYNC_URL))) {
            qCWarning(lcDrivePreferencesWidget) << "QDesktopServices::openUrl failed for " << link;
            CustomMessageBox msgBox(
                        QMessageBox::Warning,
                        tr("Unable to open link %1.").arg(link),
                        QMessageBox::Ok, this);
            msgBox.exec();
        }
    }
}*/

void DrivePreferencesWidget::onErrorsWidgetClicked() {
    emit displayErrors(_driveDbId);
}

void DrivePreferencesWidget::onBigFoldersWarningWidgetClicked() {
    EnableStateHolder _(this);

    std::unordered_map<int, std::pair<SyncInfoClient, QSet<QString>>> syncsUndecidedMap;
    for (const auto &syncInfoMapElt: _gui->syncInfoMap()) {
        if (syncInfoMapElt.second.driveDbId() == _driveDbId) {
            QSet<QString> tmpSet;
            ExitCode exitCode = GuiRequests::getSyncIdSet(syncInfoMapElt.first, SyncNodeType::UndecidedList, tmpSet);
            syncsUndecidedMap[syncInfoMapElt.first] = {syncInfoMapElt.second, tmpSet};
            if (exitCode != ExitCode::Ok) {
                qCWarning(lcDrivePreferencesWidget()) << "Error in Requests::getSyncIdSet";
                return;
            }
        }
    }

    const auto driveInfoIt = _gui->driveInfoMap().find(_driveDbId);
    if (driveInfoIt == _gui->driveInfoMap().end()) {
        qCWarning(lcDrivePreferencesWidget()) << "Drive not found in drive map for driveDbId=" << _driveDbId;
        return;
    }

    BigFoldersDialog dialog(syncsUndecidedMap, driveInfoIt->second, this);
    if (dialog.exec() == QDialog::Accepted) {
        if (!updateSelectiveSyncList(dialog.mapWhiteListedSubFolders())) {
            qCWarning(lcDrivePreferencesWidget) << "Error in updateSelectiveSyncList";
        }

        _displayBigFoldersWarningWidget->setVisible(existUndecidedSet());
        refreshFoldersBlocs();
    }
}

void DrivePreferencesWidget::onAddLocalFolder(bool checked) {
    Q_UNUSED(checked)

    MatomoClient::sendEvent("drivePreferences", MatomoEventAction::Click, "synchronizeALocalFolder");
    EnableStateHolder _(this);

    const QString addFolderError = tr("New local folder synchronization failed!");
    bool liteSync = false;
    QString localFolderPath;
    QString serverFolderBasePath;
    QList<QPair<QString, QString>> serverFolderList;
    qint64 serverFolderSize = 0;
    QString serverFolderName;
    QString serverFolderPath;
    QString serverFolderNodeId;
    bool hasSubfolders = false;
    QSet<QString> blackList;
    QSet<QString> whiteList;
    QString localFolderName;
    qint64 localFolderSize = 0;
    AddFolderStep nextStep = SelectLocalFolder;

    while (true) {
        if (nextStep == SelectLocalFolder) {
            LocalFolderDialog localFolderDialog(_gui, localFolderPath, this);

            localFolderDialog.setLiteSync(liteSync);
            connect(&localFolderDialog, &LocalFolderDialog::openFolder, this, &DrivePreferencesWidget::onOpenFolder);
            if (localFolderDialog.execAndMoveToCenter(KDC::GuiUtility::getTopLevelWidget(this)) == QDialog::Rejected) {
                break;
            }

            localFolderPath = localFolderDialog.localFolderPath();
            QFileInfo localFolderInfo(localFolderPath);
            localFolderName = localFolderInfo.baseName();
            localFolderSize = KDC::GuiUtility::folderSize(localFolderPath);
            liteSync = localFolderDialog.folderCompatibleWithLiteSync();
            qCDebug(lcDrivePreferencesWidget) << "Local folder selected: " << localFolderPath;
            MatomoClient::sendVisit(MatomoNameField::PG_Parameters_NewSync_LocalFolder);
            nextStep = SelectServerBaseFolder;
        }

        if (nextStep == SelectServerBaseFolder) {
            ServerBaseFolderDialog serverBaseFolderDialog(_gui, _driveDbId, localFolderName, localFolderPath, this);
            int ret = serverBaseFolderDialog.execAndMoveToCenter(KDC::GuiUtility::getTopLevelWidget(this));
            if (ret == QDialog::Rejected) {
                qCDebug(lcDrivePreferencesWidget) << "Cancel: " << nextStep;
                break;
            } else if (ret == -1) {
                // Go back
                nextStep = SelectLocalFolder;
                continue;
            }

            serverFolderBasePath = serverBaseFolderDialog.serverFolderBasePath();
            serverFolderList = serverBaseFolderDialog.serverFolderList();

            if (!serverFolderList.empty()) {
                serverFolderName = serverFolderList[serverFolderList.size() - 1].first;
                serverFolderNodeId = serverFolderList[serverFolderList.size() - 1].second;

                for (qsizetype i = 0; i < serverFolderList.size(); i++) {
                    serverFolderPath += dirSeparator + serverFolderList[i].first;
                }
            } else {
                const auto driveInfoIt = _gui->driveInfoMap().find(_driveDbId);
                if (driveInfoIt == _gui->driveInfoMap().end()) {
                    qCWarning(lcDrivePreferencesWidget()) << "Drive not found in drive map for driveDbId=" << _driveDbId;
                    return;
                }

                serverFolderName = driveInfoIt->second.name();
            }
            qCDebug(lcDrivePreferencesWidget) << "Server folder selected: " << serverFolderName;
            MatomoClient::sendVisit(MatomoNameField::PG_Parameters_NewSync_RemoteFolder);
            nextStep = SelectServerFolders;
        }

        if (nextStep == SelectServerFolders) {
            if (!serverFolderNodeId.isEmpty()) {
                ExitCode exitCode;
                QList<NodeInfo> nodeInfoList;
                exitCode = GuiRequests::getSubFolders(_driveDbId, serverFolderNodeId, nodeInfoList);
                if (exitCode != ExitCode::Ok) {
                    qCWarning(lcDrivePreferencesWidget()) << "Error in Requests::getSubFolders";
                    CustomMessageBox msgBox(QMessageBox::Warning, addFolderError, QMessageBox::Ok, this);
                    msgBox.setDefaultButton(QMessageBox::Ok);
                    msgBox.exec();
                    break;
                }

                if (!nodeInfoList.empty()) {
                    ServerFoldersDialog serverFoldersDialog(_gui, _driveDbId, serverFolderName, serverFolderNodeId, this);
                    int ret = serverFoldersDialog.exec(KDC::GuiUtility::getTopLevelWidget(this)->pos());
                    if (ret == QDialog::Rejected) {
                        qCDebug(lcDrivePreferencesWidget) << "Cancel: " << nextStep;
                        break;
                    } else if (ret == -1) {
                        // Go back
                        nextStep = SelectServerBaseFolder;
                        continue;
                    }

                    serverFolderSize = serverFoldersDialog.selectionSize();
                    blackList = serverFoldersDialog.getBlacklist();
                    whiteList = serverFoldersDialog.getWhiteList();
                    qCDebug(lcDrivePreferencesWidget) << "Server subfolders selected";
                }
            }
            nextStep = Confirm;
        }

        if (nextStep == Confirm) {
            MatomoClient::sendVisit(MatomoNameField::PG_Parameters_NewSync_Summary);
            int driveId;
            ExitCode exitCode = GuiRequests::getDriveIdFromDriveDbId(_driveDbId, driveId);
            if (exitCode != ExitCode::Ok) {
                qCWarning(lcDrivePreferencesWidget()) << "Error in GuiRequests::getDriveIdFromDriveDbId";
                return;
            }

            ConfirmSynchronizationDialog confirmSynchronizationDialog(_gui, _userDbId, driveId, serverFolderNodeId,
                                                                      localFolderName, localFolderSize, serverFolderName,
                                                                      serverFolderSize, this);
            int ret = confirmSynchronizationDialog.execAndMoveToCenter(KDC::GuiUtility::getTopLevelWidget(this));
            if (ret == QDialog::Rejected) {
                qCDebug(lcDrivePreferencesWidget) << "Cancel: " << nextStep;
                break;
            } else if (ret == -1) {
                // Go back
                nextStep = hasSubfolders ? SelectServerFolders : SelectServerBaseFolder;
                continue;
            }

            // Setup local folder
            const QDir localFolderDir(localFolderPath);
            if (localFolderDir.exists()) {
                KDC::CommonGuiUtility::setupFavLink(localFolderPath);
                qCDebug(lcDrivePreferencesWidget) << "Local folder setup: " << localFolderPath;
            } else {
                qCDebug(lcDrivePreferencesWidget) << "Local folder doesn't exist anymore: " << localFolderPath;
                CustomMessageBox msgBox(QMessageBox::Warning, addFolderError, QMessageBox::Ok, this);
                msgBox.setDefaultButton(QMessageBox::Ok);
                msgBox.exec();
                break;
            }

            if (serverFolderNodeId.isEmpty()) {
                // Create missing server folders
                ExitCode exitCode = GuiRequests::createMissingFolders(_driveDbId, serverFolderList, serverFolderNodeId);
                if (exitCode != ExitCode::Ok) {
                    qCDebug(lcDrivePreferencesWidget) << "Error in Requests::createDir for driveDbId=" << _driveDbId;
                    CustomMessageBox msgBox(QMessageBox::Warning, addFolderError, QMessageBox::Ok, this);
                    msgBox.setDefaultButton(QMessageBox::Ok);
                    msgBox.exec();
                    break;
                }
            }

            // Add folder to synchronization
            if (!addSync(localFolderPath, liteSync, serverFolderPath, serverFolderNodeId, blackList, whiteList)) {
                CustomMessageBox msgBox(QMessageBox::Warning, addFolderError, QMessageBox::Ok, this);
                msgBox.setDefaultButton(QMessageBox::Ok);
                msgBox.exec();
            }

            break;
        }
    }
}

void DrivePreferencesWidget::onLiteSyncSwitchSyncChanged(int syncDbId, bool activate) {
    if (activate) {
        askEnableLiteSync([this, &syncDbId](bool enable) {
            if (enable) {
                auto syncInfoMapIt = _gui->syncInfoMap().find(syncDbId);
                if (syncInfoMapIt != _gui->syncInfoMap().end()) {
                    if (switchVfsOn(syncInfoMapIt->first)) {
                        CustomMessageBox msgBox(QMessageBox::Information, tr("Lite Sync activated."), QMessageBox::Ok, this);
                        msgBox.execAndMoveToCenter(KDC::GuiUtility::getTopLevelWidget(this));
                    } else {
                        qCWarning(lcDrivePreferencesWidget()) << "Error when switching vfs on";
                        CustomMessageBox msgBox(QMessageBox::Information, tr("Lite Sync activation failed."), QMessageBox::Ok,
                                                this);
                        msgBox.execAndMoveToCenter(KDC::GuiUtility::getTopLevelWidget(this));
                    }
                }
            }
        });
    } else {
        askDisableLiteSync(
                [this, &syncDbId](bool disable, bool diskSpaceWarning) {
                    if (disable) {
                        auto syncInfoMapIt = _gui->syncInfoMap().find(syncDbId);
                        if (syncInfoMapIt != _gui->syncInfoMap().end()) {
                            if (switchVfsOff(syncInfoMapIt->first, diskSpaceWarning)) {
                                CustomMessageBox msgBox(QMessageBox::Information, tr("Lite Sync deactivated."), QMessageBox::Ok,
                                                        this);
                                (void) msgBox.execAndMoveToCenter(KDC::GuiUtility::getTopLevelWidget(this));
                            } else {
                                CustomMessageBox msgBox(QMessageBox::Information, tr("Lite Sync deactivation failed."),
                                                        QMessageBox::Ok, this);
                                (void) msgBox.execAndMoveToCenter(KDC::GuiUtility::getTopLevelWidget(this));
                            }
                        }
                    }
                },
                syncDbId);
    }
}

void DrivePreferencesWidget::onNotificationsSwitchClicked(bool checked) {
    MatomoClient::sendEvent("drivePreferences", MatomoEventAction::Click, "notificationSwitch", checked ? 1 : 0);
    const auto driveInfoMapIt = _gui->driveInfoMap().find(_driveDbId);
    if (driveInfoMapIt == _gui->driveInfoMap().end()) {
        qCWarning(lcDrivePreferencesWidget()) << "Drive not found in drive map for driveDbId=" << _driveDbId;
        return;
    }

    driveInfoMapIt->second.setNotifications(checked);

    ExitCode exitCode = GuiRequests::updateDrive(driveInfoMapIt->second);
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcDrivePreferencesWidget()) << "Error in Requests::updateDrive";
    }
}

void DrivePreferencesWidget::onErrorAdded() {
    _displayErrorsWidget->setVisible(true);
}

void DrivePreferencesWidget::onRemoveDrive(bool checked) {
    Q_UNUSED(checked)

    MatomoClient::sendEvent("drivePreferences", MatomoEventAction::Click, "removeDrive");
    const auto driveInfoIt = _gui->driveInfoMap().find(_driveDbId);
    if (driveInfoIt == _gui->driveInfoMap().end()) {
        qCWarning(lcDrivePreferencesWidget()) << "Drive not found in drive map for driveDbId=" << _driveDbId;
        return;
    }

    emit removeDrive(driveInfoIt->second.dbId());
}

void DrivePreferencesWidget::onDriveBeingRemoved() {
    const auto driveInfoIt = _gui->driveInfoMap().find(_driveDbId);
    assert(driveInfoIt != _gui->driveInfoMap().cend());
    driveInfoIt->second.setIsBeingDeleted(true);

    // Lock all GUI drive-related actions during drive deletion.
    for (auto *child: findChildren<QWidget *>()) {
        GuiUtility::setEnabledRecursively<QWidget>(child, false);
    }

    const QList<PreferencesBlocWidget *> folderBlocList = findChildren<PreferencesBlocWidget *>(folderBlocName);
    for (PreferencesBlocWidget *folderBloc: folderBlocList) {
        folderBloc->setToolTipsEnabled(false);
    }

    setCustomToolTipText(tr("This drive is being deleted."));
    update();
}

void DrivePreferencesWidget::onSearch() {
    const QString searchString = _searchBox->text();
    _searchResultModel.clear();
    _searchResultView->setVisible(false);
    if (searchString.isEmpty()) {
        _searchProgressLabel->setVisible(false);
        return;
    }
    _searchProgressLabel->setVisible(true);
    _searchProgressLabel->setText("Searching...");

    QList<SearchInfo> searchInfos;
    bool hasMore = false; // To be used for the new UI
    QString cursor; // To be used for the new UI
    (void) GuiRequests::searchItemInDrive(_driveDbId, searchString, searchInfos, hasMore, cursor);
    if (searchInfos.empty()) {
        _searchProgressLabel->setText("No items found!");
        return;
    }
    _searchProgressLabel->setVisible(false);

    _searchResultModel.setSearchItems(searchInfos);
    _searchResultView->setVisible(true);
}

void DrivePreferencesWidget::onSearchItemDoubleClicked(const QModelIndex &index) {
    const auto id = _searchResultModel.id(index.row());

    int driveId = 0;

    if (const auto exitCode = GuiRequests::getDriveIdFromDriveDbId(_driveDbId, driveId); exitCode != ExitCode::Ok) {
        qCWarning(lcDrivePreferencesWidget()) << "Error in GuiRequests::getDriveIdFromDriveDbId";
        return;
    }

    NodeInfo nodeInfo;

    if (const auto exitCode = GuiRequests::getNodeInfo(_userDbId, driveId, QString::fromStdString(id), nodeInfo, true);
        exitCode != ExitCode::Ok) {
        qCWarning(lcDrivePreferencesWidget()) << "Error in GuiRequests::getNodeInfo";
    }

    for (const auto &[_, sync]: _gui->syncInfoMap()) {
        // Look for item on local replica.
        const QString filepath = sync.localPath() + nodeInfo.path();
        if (QFileInfo::exists(filepath)) {
            GuiUtility::openFolder(filepath);
            return;
        }
    }

    // If not found on local replica, open the web version.
    QString privateLink;
    if (const auto exitCode = GuiRequests::getPrivateLinkUrl(_driveDbId, QString::fromStdString(id), privateLink);
        exitCode != ExitCode::Ok) {
        qCWarning(lcDrivePreferencesWidget()) << "Error in GuiRequests::getPrivateLinkUrl";
    }

    if (privateLink.isEmpty()) {
        CustomMessageBox msgBox(QMessageBox::Warning, tr("Impossible to open item %1").arg(nodeInfo.path()), QMessageBox::Ok,
                                this);
        (void) msgBox.exec();
        return;
    }

    QDesktopServices::openUrl(privateLink);
}

void DrivePreferencesWidget::onUnsyncTriggered(int syncDbId) {
    EnableStateHolder _(this);

    const auto &syncInfoMapIt = _gui->syncInfoMap().find(syncDbId);
    if (syncInfoMapIt == _gui->syncInfoMap().end()) {
        qCWarning(lcDrivePreferencesWidget()) << "Sync not found in syncInfoMap for syncDbId=" << syncDbId;
        return;
    }

    CustomMessageBox msgBox(QMessageBox::Question,
                            tr("Do you really want to stop syncing the folder <i>%1</i> ?<br>"
                               "<b>Note:</b> This will <b>not</b> delete any files.")
                                    .arg(syncInfoMapIt->second.localPath()),
                            QMessageBox::NoButton, this);
    msgBox.addButton(tr("REMOVE FOLDER SYNC CONNECTION"), QMessageBox::Yes);
    msgBox.addButton(tr("CANCEL"), QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);

    if (msgBox.exec() == QMessageBox::Yes) {
        PreferencesBlocWidget *folderBloc = (PreferencesBlocWidget *) sender()->parent();
        if (!folderBloc) {
            return;
        }

        // Disable GUI sync-related actions.
        folderBloc->setEnabledRecursively(false);

        const auto &syncInfoMapIt = _gui->syncInfoMap().find(syncDbId);
        if (syncInfoMapIt != _gui->syncInfoMap().end()) {
            CommonGuiUtility::removeDirIcon(syncInfoMapIt->second.localPath());
        }

        // Remove sync
        const ExitCode exitCode = GuiRequests::deleteSync(syncDbId);
        if (exitCode != ExitCode::Ok) {
            qCWarning(lcDrivePreferencesWidget()) << "Error in Requests::removeSync";
            CustomMessageBox msgBox(QMessageBox::Warning,
                                    tr("Failed to stop syncing the folder <i>%1</i>.").arg(syncInfoMapIt->second.localPath()),
                                    QMessageBox::Ok, this);

            // Re-enable GUI sync-related actions.
            folderBloc->setEnabledRecursively(true);
        }
    }
}

void DrivePreferencesWidget::onDisplayFolderDetail(int syncDbId, bool display) {
    if (syncDbId) {
        FolderTreeItemWidget *treeItemWidget = blocTreeItemWidget((PreferencesBlocWidget *) sender()->parent());
        if (treeItemWidget) {
            if (display) {
                setCursor(Qt::WaitCursor);
                treeItemWidget->setSyncDbId(syncDbId);
                treeItemWidget->loadSubFolders();
            } else {
                QFrame *separatorFrame = blocSeparatorFrame((PreferencesBlocWidget *) sender()->parent());
                if (!separatorFrame) {
                    return;
                }

                treeItemWidget->setVisible(false);
                separatorFrame->setVisible(false);
            }
        }
    }
}

void DrivePreferencesWidget::onOpenFolder(const QString &filePath) {
    emit openFolder(filePath);
}

void DrivePreferencesWidget::onSubfoldersLoaded(const bool error, const ExitCause, const bool empty) {
    setCursor(Qt::ArrowCursor);
    if (error || empty) {
        FolderItemWidget *itemWidget = blocItemWidget((PreferencesBlocWidget *) sender()->parent());
        if (!itemWidget) {
            return;
        }

        emit itemWidget->displayFolderDetailCanceled();

        if (error) {
            CustomMessageBox msgBox(QMessageBox::Warning, tr("An error occurred while loading the list of subfolders."),
                                    QMessageBox::Ok, this);
            msgBox.setDefaultButton(QMessageBox::Ok);
            msgBox.exec();
        }
    } else {
        FolderTreeItemWidget *treeItemWidget = (FolderTreeItemWidget *) sender();
        if (!treeItemWidget) {
            return;
        }

        QFrame *separatorFrame = blocSeparatorFrame((PreferencesBlocWidget *) sender()->parent());
        if (!separatorFrame) {
            return;
        }

        treeItemWidget->setVisible(true);
        separatorFrame->setVisible(true);
    }
}

void DrivePreferencesWidget::onNeedToSave(bool isFolderItemBlackListed) {
    // Show update widget
    FolderItemWidget *itemWidget = blocItemWidget((PreferencesBlocWidget *) sender()->parent());
    if (itemWidget) {
        itemWidget->setUpdateWidgetVisible(true);
        itemWidget->setUpdateWidgetLabelVisible(isFolderItemBlackListed);
    }
}

void DrivePreferencesWidget::onCancelUpdate(int syncDbId) {
    FolderTreeItemWidget *treeItemWidget = blocTreeItemWidget((PreferencesBlocWidget *) sender()->parent());
    if (treeItemWidget) {
        QLOG_IF_FAIL(treeItemWidget->syncDbId() == syncDbId);
        treeItemWidget->loadSubFolders();

        // Hide update widget
        FolderItemWidget *itemWidget = (FolderItemWidget *) sender();
        if (itemWidget) {
            itemWidget->setUpdateWidgetVisible(false);
            itemWidget->setUpdateWidgetLabelVisible(false);
        }
    }
}

void DrivePreferencesWidget::onValidateUpdate(int syncDbId) {
    FolderTreeItemWidget *treeItemWidget = blocTreeItemWidget((PreferencesBlocWidget *) sender()->parent());
    if (treeItemWidget) {
        QLOG_IF_FAIL(treeItemWidget->syncDbId() == syncDbId);
        _displayBigFoldersWarningWidget->setVisible(false);

        QSet<QString> oldUndecidedSet;
        ExitCode exitCode = GuiRequests::getSyncIdSet(syncDbId, SyncNodeType::UndecidedList, oldUndecidedSet);
        if (exitCode != ExitCode::Ok) {
            qCWarning(lcDrivePreferencesWidget()) << "Error in Requests::getSyncIdSet";
            return;
        }

        QSet<QString> oldBlackSet;
        exitCode = GuiRequests::getSyncIdSet(syncDbId, SyncNodeType::BlackList, oldBlackSet);
        if (exitCode != ExitCode::Ok) {
            qCWarning(lcDrivePreferencesWidget()) << "Error in Requests::getSyncIdSet";
            return;
        }

        // Clear the undecided list
        exitCode = GuiRequests::setSyncIdSet(syncDbId, SyncNodeType::UndecidedList, QSet<QString>());
        if (exitCode != ExitCode::Ok) {
            qCWarning(lcDrivePreferencesWidget()) << "Error in Requests::setSyncIdSet";
            return;
        }

        // Update the blacklist
        const QSet<QString> blackSet = treeItemWidget->createBlackSet();
        if (!GuiUtility::checkBlacklistSize(blackSet.size(), this)) return;
        exitCode = GuiRequests::setSyncIdSet(syncDbId, SyncNodeType::BlackList, blackSet);
        if (exitCode != ExitCode::Ok) {
            qCWarning(lcDrivePreferencesWidget()) << "Error in Requests::setSyncIdSet";
            return;
        }

        // Update the whitelist
        QSet<QString> whiteSet = (oldUndecidedSet + oldBlackSet) - blackSet;
        exitCode = GuiRequests::setSyncIdSet(syncDbId, SyncNodeType::WhiteList, whiteSet);
        if (exitCode != ExitCode::Ok) {
            qCWarning(lcDrivePreferencesWidget()) << "Error in Requests::setSyncIdSet";
            return;
        }

        GuiRequests::propagateSyncListChange(syncDbId, true);

        // Hide update widget
        FolderItemWidget *itemWidget = (FolderItemWidget *) sender();
        if (itemWidget) {
            itemWidget->setUpdateWidgetVisible(false);
            itemWidget->setUpdateWidgetLabelVisible(false);
        }
    }
}

void DrivePreferencesWidget::onNewBigFolderDiscovered(int syncDbId, const QString &path) {
    Q_UNUSED(syncDbId)
    Q_UNUSED(path)

    _displayBigFoldersWarningWidget->setVisible(existUndecidedSet());
    refreshFoldersBlocs();
}

void DrivePreferencesWidget::onUndecidedListsCleared() {
    _displayBigFoldersWarningWidget->setVisible(existUndecidedSet());
    refreshFoldersBlocs();
}

void DrivePreferencesWidget::retranslateUi() {
    _displayErrorsWidget->setText(tr("Synchronization errors and information."));
    _displayBigFoldersWarningWidget->setText(tr("Some folders were not synchronized because they are too large."));
    _foldersLabel->setText(tr("Folders"));
    _addLocalFolderButton->setText(tr("Synchronize a local folder"));
    _notificationsLabel->setText(tr("Notifications"));
    _notificationsTitleLabel->setText(tr("Enable the notifications for this kDrive"));
    _notificationsDescriptionLabel->setText(
            tr("A notification will be displayed as soon as a new folder has been synchronized or modified"));
    _connectedWithLabel->setText(tr("Connected with"));
    _removeDriveButton->setToolTip(tr("Remove all synchronizations"));
    _searchLabel->setText(tr("Search in your kDrive"));
    _searchButton->setText(tr("Search"));
}


} // namespace KDC
