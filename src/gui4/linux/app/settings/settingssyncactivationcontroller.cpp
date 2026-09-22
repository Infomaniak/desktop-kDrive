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

bool SettingsSyncActivationController::canValidate() const {
    if (_busy || _preparing || !_visible || _reconciliationBlockedKeys.contains(_key)) {
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
    if (_syncCreationPending) {
        return;
    }

    const bool targetChangeNeedsNotification = _preparing;
    ++_requestGeneration;
    _reconciliationPending = false;
    _reconciliationForActivation = false;
    _key = {
            .userDbId = static_cast<UserDbId>(userDbId),
            .accountId = static_cast<AccountId>(accountId),
            .driveId = static_cast<DriveId>(driveId),
    };
    _config = {};
    _driveName.clear();
    _driveColor = AppConstants::Drive::defaultColor();
    _localFolderErrorId.clear();
    _operationErrorId.clear();

    const auto availableDrive = _appCache.availableDrive(_key);
    if (!availableDrive || _appCache.isAvailableDriveConfigured(_key)) {
        qCWarning(lcSettingsSyncActivationController)
                << "Drive activation ignored because the target is no longer available | userDbId:" << _key.userDbId
                << "/ accountId:" << _key.accountId << "/ driveId:" << _key.driveId;
        resetTarget();
        emit presentationChanged();
        return;
    }

    _driveName = QString::fromStdString(availableDrive->name());
    const QColor driveColor{QString::fromStdString(availableDrive->color())};
    _driveColor = driveColor.isValid() ? driveColor : AppConstants::Drive::defaultColor();
    setPage(Page::DriveConfiguration);
    setPreparing(true);
    // The target may change while preparation is already active, in which case setPreparing() emits nothing.
    if (targetChangeNeedsNotification) {
        emit presentationChanged();
    }
    if (_reconciliationBlockedKeys.contains(_key)) {
        _reconciliationPending = true;
        _reconciliationForActivation = true;
        _cachePopulator.reconcile();
        return;
    }
    requestDefaultFolder();
}

bool SettingsSyncActivationController::targets(const qint64 userDbId, const qint64 accountId, const qint64 driveId) const {
    return _key.userDbId == static_cast<UserDbId>(userDbId) && _key.accountId == static_cast<AccountId>(accountId) &&
           _key.driveId == static_cast<DriveId>(driveId);
}

void SettingsSyncActivationController::cancelCurrentPage() {
    if (_busy || _preparing) {
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
    if (_syncCreationPending) {
        if (_visible) {
            _visible = false;
            emit visibleChanged();
        }
        return;
    }

    close();
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
    if (_busy || _preparing || !_visible) {
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
    if (_busy || _preparing || !_visible || path.isEmpty()) {
        return;
    }

    setBusy(true);
    setLocalFolderErrorId({});
    setOperationErrorId({});
    const uint64_t generation = ++_requestGeneration;
    const QPointer self(this);
    _commService.requestIsPathValidForNewSync(
            QStr2Path(path), SyncConfiguration::Classic, [self, generation, path](const ExitInfo &exitInfo, const bool valid) {
                if (!self || generation != self->_requestGeneration) {
                    return;
                }
                self->setBusy(false);
                if (!self->targetStillAvailable()) {
                    self->close();
                    return;
                }
                if (!exitInfo || !valid) {
                    self->setLocalFolderErrorId(u"teachingTipInvalidFolderContent"_s);
                    return;
                }
                self->_config.localPath = path;
                self->_config.usesDefaultLocalPath = QDir::cleanPath(self->_config.defaultLocalPath) == path;
                emit self->presentationChanged();
            });
}

void SettingsSyncActivationController::returnToDefaultFolder() {
    if (_busy || _preparing || !_visible) {
        return;
    }
    setBusy(true);
    setLocalFolderErrorId({});
    setOperationErrorId({});
    requestDefaultFolder();
}

void SettingsSyncActivationController::selectFolders() {
    if (_busy || _preparing || !_visible || !targetStillAvailable()) {
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

void SettingsSyncActivationController::requestDefaultFolder() {
    const auto availableDrive = _appCache.availableDrive(_key);
    if (!availableDrive || _appCache.isAvailableDriveConfigured(_key)) {
        if (_visible) {
            setBusy(false);
            setLocalFolderErrorId(u"teachingTipInvalidFolderContent"_s);
        } else {
            setPreparing(false);
            resetTarget();
        }
        return;
    }

    const uint64_t generation = ++_requestGeneration;
    const QPointer self(this);
    _commService.requestFindGoodPathForNewSync(CommonUtility::str2CommString(availableDrive->name()),
                                               [self, generation](const ExitInfo &exitInfo, const GoodPathResult &result) {
                                                   if (!self || generation != self->_requestGeneration) {
                                                       return;
                                                   }
                                                   self->handleDefaultFolderProposal(generation, exitInfo, result);
                                               });
}

void SettingsSyncActivationController::handleDefaultFolderProposal(const uint64_t generation, const ExitInfo &exitInfo,
                                                                   const GoodPathResult &result) {
    if (generation != _requestGeneration) {
        return;
    }
    const QString rawPath = Path2QStr(result.goodPath);
    const QString path = rawPath.isEmpty() ? QString{} : QDir::cleanPath(rawPath);
    if (!targetStillAvailable()) {
        if (_visible) {
            setBusy(false);
            close();
        } else {
            setPreparing(false);
            resetTarget();
            emit presentationChanged();
        }
        return;
    }
    if (!exitInfo || path.isEmpty()) {
        if (!exitInfo) {
            _serviceEventBus.notifyGenericError(exitInfo, RequestNum::UTILITY_FINDGOODPATHFORNEWSYNC);
        } else {
            emit _serviceEventBus.genericErrorOccurred();
        }
        if (_visible) {
            setBusy(false);
            setLocalFolderErrorId(u"teachingTipInvalidFolderContent"_s);
        } else {
            setPreparing(false);
            resetTarget();
        }
        return;
    }

    _config.localPath = path;
    _config.defaultLocalPath = path;
    _config.usesDefaultLocalPath = true;
    if (_visible) {
        setBusy(false);
    } else {
        setPreparing(false);
        _visible = true;
        emit visibleChanged();
    }
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

    setBusy(true);
    setOperationErrorId({});
    _syncCreationPending = true;
    const uint64_t generation = ++_requestGeneration;
    const QPointer self(this);
    _commService.requestSyncAdd(request, [self, generation](const ExitInfo &exitInfo, const BaseSync &) {
        if (!self || generation != self->_requestGeneration) {
            return;
        }
        if (exitInfo) {
            self->close();
            return;
        }

        self->_serviceEventBus.notifyGenericError(exitInfo, RequestNum::SYNC_ADD);
        self->_reconciliationPending = true;
        self->_cachePopulator.reconcile();
    });
}

void SettingsSyncActivationController::handleReconciliationFinished(const bool succeeded) {
    if (succeeded) {
        _reconciliationBlockedKeys.clear();
    }
    if (!_reconciliationPending) {
        return;
    }

    _reconciliationPending = false;
    if (_reconciliationForActivation) {
        _reconciliationForActivation = false;
        if (!succeeded) {
            emit _serviceEventBus.genericErrorOccurred();
            setPreparing(false);
            resetTarget();
            emit presentationChanged();
            return;
        }
        if (!targetStillAvailable()) {
            close();
            return;
        }

        requestDefaultFolder();
        return;
    }

    _syncCreationPending = false;
    if (!succeeded) {
        (void) _reconciliationBlockedKeys.insert(_key);
        if (!_visible) {
            close();
            return;
        }
        setBusy(false);
        setOperationErrorId(u"unexpectedErrorTeachingTipContent"_s);
        return;
    }
    if (_appCache.isAvailableDriveConfigured(_key)) {
        close();
        return;
    }
    if (!targetStillAvailable()) {
        close();
        return;
    }
    if (!_visible) {
        close();
        return;
    }
    setBusy(false);
    setOperationErrorId(u"unexpectedErrorTeachingTipContent"_s);
}

void SettingsSyncActivationController::handleTargetStateChanged() {
    if ((!_visible && !_preparing) || _busy || targetStillAvailable()) {
        return;
    }

    if (_visible) {
        if (_appCache.isAvailableDriveConfigured(_key)) {
            (void) _reconciliationBlockedKeys.erase(_key);
        }
        close();
        return;
    }

    ++_requestGeneration;
    setPreparing(false);
    resetTarget();
    emit presentationChanged();
}

void SettingsSyncActivationController::setPage(const Page page) {
    if (_page == page) {
        return;
    }
    _page = page;
    emit pageChanged();
}

void SettingsSyncActivationController::setPreparing(const bool preparing) {
    if (_preparing == preparing) {
        return;
    }
    _preparing = preparing;
    emit presentationChanged();
}

void SettingsSyncActivationController::setBusy(const bool busy) {
    if (_busy == busy) {
        return;
    }
    _busy = busy;
    emit presentationChanged();
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

void SettingsSyncActivationController::close() {
    ++_requestGeneration;
    _reconciliationPending = false;
    _reconciliationForActivation = false;
    _syncCreationPending = false;
    _preparing = false;
    _busy = false;
    _localFolderErrorId.clear();
    _operationErrorId.clear();
    if (_visible) {
        _visible = false;
        emit visibleChanged();
    }
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
