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

#include "onboardingsynccreationcoordinator.h"

#include "app/cache/appcache.h"
#include "app/cache/onboardingstate.h"
#include "app/onboarding/onboardingflowcontroller.h"
#include "app/services/cachepopulator.h"
#include "app/services/commservice.h"
#include "app/services/serviceeventbus.h"
#include "libcommon/utility/types.h"

#include <QDesktopServices>
#include <QDir>
#include <QLoggingCategory>
#include <QSet>
#include <QUrl>

namespace KDC {

namespace {
Q_LOGGING_CATEGORY(lcOnboardingSyncCreationCoordinator, "gui.v4.onboardingsynccreationcoordinator", QtInfoMsg)
}

OnboardingSyncCreationCoordinator::OnboardingSyncCreationCoordinator(OnboardingFlowController &flowController,
                                                                     OnboardingState &onboardingState, AppCache &appCache,
                                                                     CommService &commService, CachePopulator &cachePopulator,
                                                                     ServiceEventBus &serviceEventBus, QObject *const parent) :
    QObject(parent),
    _flowController(flowController),
    _onboardingState(onboardingState),
    _appCache(appCache),
    _commService(commService),
    _cachePopulator(cachePopulator),
    _serviceEventBus(serviceEventBus) {
    (void) connect(&_flowController, &OnboardingFlowController::driveSelectionContinueRequested, this,
                   &OnboardingSyncCreationCoordinator::startSynchronization);
    (void) connect(&_flowController, &OnboardingFlowController::synchronizationRetryRequested, this,
                   &OnboardingSyncCreationCoordinator::createNextSynchronization);
    (void) connect(&_flowController, &OnboardingFlowController::synchronizedFoldersOpenRequested, this,
                   &OnboardingSyncCreationCoordinator::openSynchronizedFolders);
    (void) connect(&_cachePopulator, &CachePopulator::bootstrapCompleted, this,
                   &OnboardingSyncCreationCoordinator::handleCacheReconciliationCompleted);
}

void OnboardingSyncCreationCoordinator::startSynchronization() {
    _pendingDriveKeys.clear();
    _createdLocalPaths.clear();

    const auto selectedDriveKeys = _onboardingState.selectedAvailableDriveKeys();
    _pendingDriveKeys.insert(_pendingDriveKeys.end(), selectedDriveKeys.begin(), selectedDriveKeys.end());

    qCInfo(lcOnboardingSyncCreationCoordinator) << "Starting onboarding sync creation | count:" << _pendingDriveKeys.size();
    _flowController.beginSynchronization();
    createNextSynchronization();
}

void OnboardingSyncCreationCoordinator::createNextSynchronization() {
    if (_pendingDriveKeys.empty()) {
        qCInfo(lcOnboardingSyncCreationCoordinator) << "All onboarding syncs created successfully";
        _flowController.completeSynchronization();
        return;
    }

    const auto &key = _pendingDriveKeys.front();
    if (const auto pendingConfig = _onboardingState.pendingSyncConfig(key);
        pendingConfig.has_value() && !pendingConfig->localPath.isEmpty()) {
        createSynchronization(key, *pendingConfig);
        return;
    }

    prepareSynchronization(key);
}

void OnboardingSyncCreationCoordinator::prepareSynchronization(const AvailableDriveKey &key) {
    const auto availableDrive = _appCache.availableDrive(key);
    if (!availableDrive.has_value() || !_onboardingState.isAvailableDriveSelected(key)) {
        qCWarning(lcOnboardingSyncCreationCoordinator)
                << "Skipping onboarding sync: available drive is no longer selectable | userDbId:" << key.userDbId
                << "/ driveId:" << key.driveId;
        discardPendingSynchronization(key);
        return;
    }

    const auto basePath = defaultLocalPath(QString::fromStdString(availableDrive->name()));
    qCInfo(lcOnboardingSyncCreationCoordinator)
            << "Requesting onboarding sync path | driveId:" << key.driveId << "/ basePath:" << basePath;
    _commService.requestFindGoodPathForNewSync(
            QStr2Path(basePath), [this, key](const ExitInfo &exitInfo, const GoodPathResult &result) {
                if (!exitInfo) {
                    _serviceEventBus.notifyGenericError(exitInfo, RequestNum::UTILITY_FINDGOODPATHFORNEWSYNC);
                    handleCreationFailure();
                    return;
                }

                if (!_onboardingState.isAvailableDriveSelected(key) || !_appCache.availableDrive(key).has_value()) {
                    qCWarning(lcOnboardingSyncCreationCoordinator)
                            << "Discarding prepared onboarding sync: drive is no longer selectable | userDbId:" << key.userDbId
                            << "/ driveId:" << key.driveId;
                    discardPendingSynchronization(key);
                    return;
                }

                PendingSyncConfig config;
                config.localPath = Path2QStr(result.goodPath);
                if (config.localPath.isEmpty()) {
                    qCWarning(lcOnboardingSyncCreationCoordinator)
                            << "Server returned an empty onboarding sync path | driveId:" << key.driveId;
                    handleCreationFailure();
                    return;
                }

                _onboardingState.setPendingSyncConfig(key, config);
                createSynchronization(key, config);
            });
}

void OnboardingSyncCreationCoordinator::createSynchronization(const AvailableDriveKey &key, const PendingSyncConfig &config) {
    if (!_onboardingState.isAvailableDriveSelected(key) || !_appCache.availableDrive(key).has_value()) {
        qCWarning(lcOnboardingSyncCreationCoordinator)
                << "Skipping onboarding sync creation: drive is no longer selectable | userDbId:" << key.userDbId
                << "/ driveId:" << key.driveId;
        discardPendingSynchronization(key);
        return;
    }

    SyncAddRequest request;
    request.userDbId = key.userDbId;
    request.accountId = key.accountId;
    request.driveId = key.driveId;
    request.localFolderPath = QStr2Path(config.localPath);
    request.serverFolderPath = QStr2Path(config.targetPath);
    request.serverFolderNodeId = QStr2Str(config.targetNodeId);
    request.liteSync = false;

    qCInfo(lcOnboardingSyncCreationCoordinator)
            << "Creating onboarding sync | driveId:" << key.driveId << "/ localPath:" << config.localPath;
    _commService.requestSyncAdd(
            request, [this, key, localPath = config.localPath](const ExitInfo &exitInfo, const SyncInfo &syncInfo) {
                if (!exitInfo) {
                    _serviceEventBus.notifyGenericError(exitInfo, RequestNum::SYNC_ADD);
                    handleCreationFailure(true);
                    return;
                }

                qCInfo(lcOnboardingSyncCreationCoordinator)
                        << "Onboarding sync created | driveId:" << key.driveId << "/ syncDbId:" << syncInfo.dbId();
                _createdLocalPaths.push_back(localPath);
                _onboardingState.unselectAvailableDrive(key);
                if (!_pendingDriveKeys.empty() && _pendingDriveKeys.front() == key) {
                    _pendingDriveKeys.pop_front();
                }
                createNextSynchronization();
            });
}

void OnboardingSyncCreationCoordinator::discardPendingSynchronization(const AvailableDriveKey &key) {
    _onboardingState.unselectAvailableDrive(key);
    if (!_pendingDriveKeys.empty() && _pendingDriveKeys.front() == key) {
        _pendingDriveKeys.pop_front();
    }
    createNextSynchronization();
}

void OnboardingSyncCreationCoordinator::handleCreationFailure(const bool cacheReconciliationRequired) {
    /*
     * IMPORTANT: a batch of onboarding sync creations is not transactional on the server.
     *
     * SYNC_ADD persists the account, drive and sync before returning its response. It also emits ACCOUNT_ADDED,
     * DRIVE_ADDED and SYNC_ADDED pushes, so every successful request has already become durable application state when a
     * later request fails. Rolling those successful creations back from the GUI would be unsafe, and replaying the whole
     * original selection would risk creating duplicate syncs or choosing new suffixed local folders.
     *
     * Account and drive creation also happen before sync creation and are not one database transaction. A failed SYNC_ADD
     * can therefore leave a newly persisted parent that was not emitted through ACCOUNT_ADDED or DRIVE_ADDED because the
     * job reports its signals only after complete success. Before exposing Retry, the coordinator asks CachePopulator to
     * rebuild the graph parent-first so a successful retry cannot emit DRIVE_ADDED/SYNC_ADDED into a cache that still lacks
     * their account parent.
     *
     * The coordinator stops at the first failure and keeps the current key plus every not-yet-attempted key in
     * _pendingDriveKeys. Keys that completed successfully were already removed from both this queue and OnboardingState.
     * The pending config of the failed key is deliberately preserved so Retry reuses the exact same local path instead of
     * asking the server for another candidate. Retry resumes from the queue front and Ready is reached only after the queue
     * is empty.
     *
     * Transport failures cannot reach this retry path: IpcClient treats every post-connection socket error as fatal. An
     * error callback here is therefore a server response, not an ambiguous timeout after a successful response was lost.
     */
    qCWarning(lcOnboardingSyncCreationCoordinator) << "Onboarding sync creation paused | remaining:" << _pendingDriveKeys.size();

    if (cacheReconciliationRequired) {
        _cacheReconciliationPending = true;
        qCInfo(lcOnboardingSyncCreationCoordinator) << "Reconciling cache after failed onboarding SYNC_ADD";
        _cachePopulator.bootstrap();
        return;
    }

    _flowController.failSynchronization();
}

void OnboardingSyncCreationCoordinator::handleCacheReconciliationCompleted() {
    if (!_cacheReconciliationPending) {
        return;
    }

    _cacheReconciliationPending = false;
    qCInfo(lcOnboardingSyncCreationCoordinator) << "Cache reconciled after failed onboarding SYNC_ADD";
    _flowController.failSynchronization();
}

void OnboardingSyncCreationCoordinator::openSynchronizedFolders() {
    QSet<QString> localPaths;
    for (const auto &syncInfo: _appCache.syncs()) {
        if (!syncInfo.localPath().isEmpty()) {
            localPaths.insert(QDir::cleanPath(syncInfo.localPath()));
        }
    }
    for (const auto &localPath: _createdLocalPaths) {
        if (!localPath.isEmpty()) {
            localPaths.insert(QDir::cleanPath(localPath));
        }
    }

    for (const auto &localPath: localPaths) {
        qCInfo(lcOnboardingSyncCreationCoordinator) << "Opening synchronized folder:" << localPath;
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(localPath))) {
            qCWarning(lcOnboardingSyncCreationCoordinator) << "Failed to open synchronized folder:" << localPath;
        }
    }

    _flowController.completeOnboarding();
}

QString OnboardingSyncCreationCoordinator::defaultLocalPath(const QString &driveName) const {
    auto normalizedName = driveName.trimmed();
    if (normalizedName.startsWith(QStringLiteral("kDrive"), Qt::CaseInsensitive)) {
        normalizedName.remove(0, QStringLiteral("kDrive").size());
        normalizedName = normalizedName.trimmed();
    }

    const auto folderName = normalizedName.isEmpty() ? QStringLiteral("kDrive") : QStringLiteral("kDrive %1").arg(normalizedName);
    return QDir(QDir::homePath()).filePath(folderName);
}

} // namespace KDC
