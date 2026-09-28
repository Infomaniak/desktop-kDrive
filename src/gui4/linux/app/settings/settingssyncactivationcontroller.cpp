/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#include "settingssyncactivationcontroller.h"

#include "app/appconstants.h"
#include "app/cache/appcache.h"
#include "app/services/cachepopulator.h"
#include "app/services/commservice.h"
#include "app/services/serviceeventbus.h"
#include "app/syncconfiguration/localpaths.h"
#include "libcommon/utility/utility.h"

#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QPointer>

using namespace Qt::StringLiterals;

namespace KDC {

namespace {
Q_LOGGING_CATEGORY(lcSettingsSyncActivationController, "gui.v4.settingssyncactivationcontroller", QtInfoMsg)
}


SettingsSyncActivationController::SettingsSyncActivationController(AppCache &appCache, CommService &commService,
                                                                   CachePopulator &cachePopulator,
                                                                   ServiceEventBus &serviceEventBus, QObject *const parent) :
    QObject(parent),
    _appCache(appCache),
    _commService(commService),
    _cachePopulator(cachePopulator),
    _serviceEventBus(serviceEventBus),
    _folderProvider(commService),
    _folderTreeModel(_folderProvider, this) {
    (void) connect(&_folderTreeModel, &RemoteFolderTreeModel::stateChanged, this,
                   &SettingsSyncActivationController::presentationChanged);
    (void) connect(&_cachePopulator, &CachePopulator::reconciliationCompleted, this,
                   [this] { handleReconciliationFinished(true); });
    (void) connect(&_cachePopulator, &CachePopulator::reconciliationFailed, this,
                   [this] { handleReconciliationFinished(false); });
    (void) connect(&_appCache, &AppCache::availableDrivesChanged, this, [this](const UserDbId userDbId) {
        if (userDbId == _key.userDbId) {
            handleTargetStateChanged();
        }
    });
    (void) connect(&_appCache, &AppCache::syncsChanged, this, &SettingsSyncActivationController::handleTargetStateChanged);
}

bool SettingsSyncActivationController::busy() const {
    switch (_state) {
        case State::CheckingFolder:
        case State::Submitting:
        case State::AwaitingConfirmation:
        case State::Reconciling:
            return true;
        case State::Idle:
        case State::Preparing:
        case State::Editing:
            return false;
    }
    return false;
}

bool SettingsSyncActivationController::canValidate() const {
    if (_state != State::Editing) {
        return false;
    }
    if (_page == Page::FolderSelection) {
        return !_folderTreeModel.loading() && !_folderTreeModel.loadFailed();
    }
    return !_config.localPath.isEmpty() && targetStillAvailable();
}

QString SettingsSyncActivationController::localFolderErrorText() const {
    return _localFolderErrorId.isEmpty() ? QString{} : qtTrId(qPrintable(_localFolderErrorId));
}

QString SettingsSyncActivationController::operationErrorText() const {
    return _operationErrorId.isEmpty() ? QString{} : qtTrId(qPrintable(_operationErrorId));
}

QString SettingsSyncActivationController::currentLocalPath() const {
    return displayLocalPath(_config.localPath);
}

void SettingsSyncActivationController::activate(const qint64 userDbId, const qint64 accountId, const qint64 driveId) {
    if (_state != State::Idle && _state != State::Preparing) {
        return;
    }

    ++_requestGeneration;
    _key = {
            .userDbId = static_cast<UserDbId>(userDbId),
            .accountId = static_cast<AccountId>(accountId),
            .driveId = static_cast<DriveId>(driveId),
    };
    _config = {};
    _localFolderErrorId.clear();
    _operationErrorId.clear();

    const auto availableDrive = _appCache.availableDrive(_key);
    if (!availableDrive || _appCache.isAvailableDriveConfigured(_key)) {
        qCWarning(lcSettingsSyncActivationController)
                << "Drive activation ignored because the target is no longer available | userDbId:" << _key.userDbId
                << "/ accountId:" << _key.accountId << "/ driveId:" << _key.driveId;
        close();
        return;
    }

    _driveName = QString::fromStdString(availableDrive->name());
    const QColor driveColor{QString::fromStdString(availableDrive->color())};
    _driveColor = driveColor.isValid() ? driveColor : AppConstants::Drive::defaultColor();
    setPage(Page::DriveConfiguration);
    // Notify unconditionally: switching targets while already preparing leaves the state unchanged.
    _state = State::Preparing;
    emit presentationChanged();
    requestDefaultFolder();
}

bool SettingsSyncActivationController::targets(const qint64 userDbId, const qint64 accountId, const qint64 driveId) const {
    return _key.userDbId == static_cast<UserDbId>(userDbId) && _key.accountId == static_cast<AccountId>(accountId) &&
           _key.driveId == static_cast<DriveId>(driveId);
}

void SettingsSyncActivationController::cancelCurrentPage() {
    if (_state != State::Editing) {
        return;
    }
    setLocalFolderErrorId({});
    setOperationErrorId({});
    if (_page == Page::FolderSelection) {
        setPage(Page::DriveConfiguration);
        emit presentationChanged();
        return;
    }
    close();
}

void SettingsSyncActivationController::dismissFromHostWindow() {
    switch (_state) {
        case State::Submitting:
        case State::AwaitingConfirmation:
        case State::Reconciling:
            hide();
            return;
        case State::Idle:
        case State::Preparing:
        case State::Editing:
        case State::CheckingFolder:
            close();
            return;
    }
}

void SettingsSyncActivationController::validateCurrentPage() {
    if (!canValidate()) {
        return;
    }
    setLocalFolderErrorId({});
    setOperationErrorId({});
    if (_page == Page::FolderSelection) {
        _config.blackList = _folderTreeModel.blackList();
        setPage(Page::DriveConfiguration);
        emit presentationChanged();
        return;
    }
    createSynchronization();
}

void SettingsSyncActivationController::requestCustomFolder() {
    if (_state != State::Editing) {
        return;
    }
    const QFileInfo currentLocation(_config.localPath);
    emit customFolderRequested(
            QUrl::fromLocalFile(currentLocation.exists() ? currentLocation.absoluteFilePath() : currentLocation.absolutePath()));
}

void SettingsSyncActivationController::notifyCustomFolderDialogClosed() {
    emit customFolderDialogClosed();
}

void SettingsSyncActivationController::applyCustomFolder(const QUrl &folderUrl) {
    const QString path = QDir::cleanPath(folderUrl.toLocalFile());
    if (_state != State::Editing || path.isEmpty()) {
        return;
    }

    setLocalFolderErrorId({});
    setOperationErrorId({});
    setState(State::CheckingFolder);
    const uint64_t generation = ++_requestGeneration;
    const QPointer self(this);
    _commService.requestIsPathValidForNewSync(
            QStr2Path(path), SyncConfiguration::Classic, [self, generation, path](const ExitInfo &exitInfo, const bool valid) {
                if (!self || generation != self->_requestGeneration) {
                    return;
                }
                if (!self->targetStillAvailable()) {
                    self->close();
                    return;
                }
                if (!exitInfo || !valid) {
                    self->setLocalFolderErrorId(u"teachingTipInvalidFolderContent"_s);
                } else {
                    self->_config.localPath = path;
                    self->_config.usesDefaultLocalPath = QDir::cleanPath(self->_config.defaultLocalPath) == path;
                }
                self->setState(State::Editing);
                emit self->presentationChanged();
            });
}

void SettingsSyncActivationController::returnToDefaultFolder() {
    if (_state != State::Editing) {
        return;
    }
    setLocalFolderErrorId({});
    setOperationErrorId({});
    setState(State::CheckingFolder);
    requestDefaultFolder();
}

void SettingsSyncActivationController::selectFolders() {
    if (_state != State::Editing || !targetStillAvailable()) {
        return;
    }
    _folderTreeModel.configure(_key.userDbId, _key.driveId, QStr2Str(_config.targetNodeId), _config.blackList);
    setOperationErrorId({});
    setPage(Page::FolderSelection);
    emit presentationChanged();
}

void SettingsSyncActivationController::retranslate() {
    _folderTreeModel.retranslate();
    emit presentationChanged();
}

bool SettingsSyncActivationController::targetStillAvailable() const {
    return _key.userDbId != 0 && _appCache.availableDrive(_key).has_value() && !_appCache.isAvailableDriveConfigured(_key);
}

/**
 * Requests a fresh default folder proposal, either before showing the editor (Preparing) or when the user returns to the
 * default folder (CheckingFolder).
 */
void SettingsSyncActivationController::requestDefaultFolder() {
    const auto availableDrive = _appCache.availableDrive(_key);
    if (!availableDrive || _appCache.isAvailableDriveConfigured(_key)) {
        close();
        return;
    }

    const uint64_t generation = ++_requestGeneration;
    const QPointer self(this);
    _commService.requestFindGoodPathForNewSync(CommonUtility::str2CommString(availableDrive->name()),
                                               [self, generation](const ExitInfo &exitInfo, const GoodPathResult &result) {
                                                   if (!self || generation != self->_requestGeneration) {
                                                       return;
                                                   }
                                                   self->handleDefaultFolderProposal(exitInfo, result);
                                               });
}

void SettingsSyncActivationController::handleDefaultFolderProposal(const ExitInfo &exitInfo, const GoodPathResult &result) {
    if (!targetStillAvailable()) {
        close();
        return;
    }

    const QString rawPath = Path2QStr(result.goodPath);
    const QString path = rawPath.isEmpty() ? QString{} : QDir::cleanPath(rawPath);
    if (!exitInfo || path.isEmpty()) {
        if (!exitInfo) {
            _serviceEventBus.notifyGenericError(exitInfo, RequestNum::UTILITY_FINDGOODPATHFORNEWSYNC);
        } else {
            emit _serviceEventBus.genericErrorOccurred();
        }
        if (_state == State::Preparing) {
            close();
            return;
        }
        setLocalFolderErrorId(u"teachingTipInvalidFolderContent"_s);
        setState(State::Editing);
        return;
    }

    _config.localPath = path;
    _config.defaultLocalPath = path;
    _config.usesDefaultLocalPath = true;
    if (!_visible) {
        _visible = true;
        emit visibleChanged();
    }
    setState(State::Editing);
    emit presentationChanged();
}

void SettingsSyncActivationController::createSynchronization() {
    if (!targetStillAvailable()) {
        close();
        return;
    }

    SyncAddRequest request;
    request.userDbId = _key.userDbId;
    request.accountId = _key.accountId;
    request.driveId = _key.driveId;
    request.localFolderPath = QStr2Path(_config.localPath);
    request.serverFolderPath = QStr2Path(_config.targetPath);
    request.serverFolderNodeId = QStr2Str(_config.targetNodeId);
    request.liteSync = false;
    request.blackList = _config.blackList;

    setOperationErrorId({});
    setState(State::Submitting);
    const uint64_t generation = ++_requestGeneration;
    const QPointer self(this);
    _commService.requestSyncAdd(request, [self, generation](const ExitInfo &exitInfo, const BaseSync &) {
        if (!self || generation != self->_requestGeneration) {
            return;
        }
        if (exitInfo) {
            self->awaitSyncAddedConfirmation();
            return;
        }

        self->_serviceEventBus.notifyGenericError(exitInfo, RequestNum::SYNC_ADD);
        // A failed SYNC_ADD may still have persisted the account, drive or sync without the matching pushes.
        self->setState(State::Reconciling);
        self->_cachePopulator.reconcile();
    });
}

/**
 * The server answers SYNC_ADD before its queued SYNC_ADDED push reaches AppCache. Until then, the target still looks
 * available, so it stays busy to prevent a duplicate SYNC_ADD. handleTargetStateChanged() closes the editor once the cache
 * no longer offers the target.
 */
void SettingsSyncActivationController::awaitSyncAddedConfirmation() {
    if (!targetStillAvailable()) {
        close();
        return;
    }

    setState(State::AwaitingConfirmation);
    hide();
}

/**
 * CachePopulator only reports the end of its latest run, so any terminal signal received while Reconciling answers this
 * request. A failed reconciliation closes the editor: the SYNC_ADD failure has already been reported.
 */
void SettingsSyncActivationController::handleReconciliationFinished(const bool succeeded) {
    if (_state != State::Reconciling) {
        return;
    }
    if (!succeeded || !_visible || !targetStillAvailable()) {
        close();
        return;
    }

    setOperationErrorId(u"unexpectedErrorTeachingTipContent"_s);
    setState(State::Editing);
}

void SettingsSyncActivationController::handleTargetStateChanged() {
    switch (_state) {
        case State::Preparing:
        case State::Editing:
        case State::AwaitingConfirmation:
            if (!targetStillAvailable()) {
                close();
            }
            return;
        case State::Idle:
        case State::CheckingFolder:
        case State::Submitting:
        case State::Reconciling:
            // Idle has no target; the other states re-check the target when their pending response arrives.
            return;
    }
}

void SettingsSyncActivationController::setState(const State state) {
    if (_state == state) {
        return;
    }
    _state = state;
    emit presentationChanged();
}

void SettingsSyncActivationController::setPage(const Page page) {
    if (_page == page) {
        return;
    }
    _page = page;
    emit pageChanged();
}

void SettingsSyncActivationController::setLocalFolderErrorId(const QString &translationId) {
    if (_localFolderErrorId == translationId) {
        return;
    }
    _localFolderErrorId = translationId;
    emit presentationChanged();
}

void SettingsSyncActivationController::setOperationErrorId(const QString &translationId) {
    if (_operationErrorId == translationId) {
        return;
    }
    _operationErrorId = translationId;
    emit presentationChanged();
}

void SettingsSyncActivationController::hide() {
    if (!_visible) {
        return;
    }
    _visible = false;
    emit visibleChanged();
}

void SettingsSyncActivationController::close() {
    ++_requestGeneration;
    _state = State::Idle;
    _localFolderErrorId.clear();
    _operationErrorId.clear();
    hide();
    resetTarget();
    emit presentationChanged();
}

void SettingsSyncActivationController::resetTarget() {
    _key = {};
    _config = {};
    _driveName.clear();
    _driveColor = AppConstants::Drive::defaultColor();
}

} // namespace KDC
