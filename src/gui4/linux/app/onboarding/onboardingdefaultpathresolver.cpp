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

#include "onboardingdefaultpathresolver.h"

#include "app/cache/appcache.h"
#include "app/cache/onboardingstate.h"
#include "app/services/commservice.h"
#include "app/services/serviceeventbus.h"
#include "app/syncconfiguration/localpaths.h"
#include "libcommon/utility/utility.h"

#include <QDir>
#include <QLoggingCategory>
#include <QPointer>

#include <algorithm>

namespace KDC {

namespace {
Q_LOGGING_CATEGORY(lcOnboardingDefaultPathResolver, "gui.v4.onboardingdefaultpathresolver", QtInfoMsg)
}

OnboardingDefaultPathResolver::OnboardingDefaultPathResolver(const AppCache &appCache, OnboardingState &onboardingState,
                                                             CommService &commService, ServiceEventBus &serviceEventBus,
                                                             QObject *const parent) :
    QObject(parent),
    _appCache(appCache),
    _onboardingState(onboardingState),
    _commService(commService),
    _serviceEventBus(serviceEventBus) {
    (void) connect(&_onboardingState, &OnboardingState::selectedAvailableDrivesChanged, this,
                   &OnboardingDefaultPathResolver::resolveMissingDefaultPaths);
}

void OnboardingDefaultPathResolver::invalidatePendingRequests() {
    ++_generation;
    _pendingKeys.clear();
    updatePendingResolutions();
}

bool OnboardingDefaultPathResolver::pathTakenByAnotherDrive(const QString &path, const AvailableDriveKey &excludedKey) const {
    return std::ranges::any_of(_onboardingState.selectedAvailableDriveKeys(),
                               [this, &path, &excludedKey](const AvailableDriveKey &key) {
                                   if (key == excludedKey) return false;
                                   const auto config = _onboardingState.pendingSyncConfig(key);
                                   return config && !config->localPath.isEmpty() &&
                                          localPathsOverlap(path, config->localPath);
                               });
}

void OnboardingDefaultPathResolver::resolveMissingDefaultPaths() {
    for (const auto &key: _onboardingState.selectedAvailableDriveKeys()) {
        if (_pendingKeys.contains(key)) continue;
        if (const auto config = _onboardingState.pendingSyncConfig(key); config && !config->defaultLocalPath.isEmpty()) continue;

        const auto availableDrive = _appCache.availableDrive(key);
        if (!availableDrive) continue;

        (void) _pendingKeys.insert(key);

        qCInfo(lcOnboardingDefaultPathResolver)
                << "Requesting default sync folder | userDbId:" << key.userDbId << "/ driveId:" << key.driveId;
        const uint64_t generation = _generation;
        const QPointer self(this);
        _commService.requestFindGoodPathForNewSync(
                CommonUtility::str2CommString(availableDrive->name()),
                [self, key, generation](const ExitInfo &exitInfo, const GoodPathResult &result) {
                    if (!self) return;
                    self->handleGoodPathResult(key, generation, exitInfo, result);
                });
    }
    // Also covers a plain deselection: its request stays in flight but no longer holds onboarding back.
    updatePendingResolutions();
}

void OnboardingDefaultPathResolver::handleGoodPathResult(const AvailableDriveKey &key, const uint64_t generation,
                                                         const ExitInfo &exitInfo, const GoodPathResult &result) {
    if (generation != _generation || !_pendingKeys.contains(key)) return;

    if (!_onboardingState.isAvailableDriveSelected(key)) {
        finishRequest(key);
        qCDebug(lcOnboardingDefaultPathResolver)
                << "Default sync folder dropped: drive unselected meanwhile | driveId:" << key.driveId;
        return;
    }

    const QString defaultPath = exitInfo ? makeUniqueLocalPath(Path2QStr(result.goodPath),
                                                               [this, &key](const QString &candidate) {
                                                                   return pathTakenByAnotherDrive(candidate, key);
                                                               })
                                         : QString{};
    if (defaultPath.isEmpty() || defaultPath == QDir::cleanPath(Path2QStr(result.goodPath))) {
        // Untouched server proposal: it was just vouched for, asking again would only cost a round trip.
        applyDefaultPath(key, defaultPath, exitInfo);
        return;
    }

    // Derived here, so the server has never seen it. Only it can tell a folder free of any sync from one still held
    // by a sync whose local folder the user deleted by hand, which `QFileInfo::exists()` reports as free.
    // The key stays pending until the answer comes back: onboarding must keep waiting for it.
    const QPointer self(this);
    _commService.requestIsPathValidForNewSync(QStr2Path(defaultPath), SyncConfiguration::Classic,
                                              [self, key, generation, defaultPath, exitInfo](const ExitInfo &checkInfo,
                                                                                             const bool valid) {
                                                  if (!self || generation != self->_generation) return;
                                                  self->applyDefaultPath(key, checkInfo && valid ? defaultPath : QString{},
                                                                         exitInfo);
                                              });
}

void OnboardingDefaultPathResolver::applyDefaultPath(const AvailableDriveKey &key, const QString &defaultPath,
                                                     const ExitInfo &exitInfo) {
    finishRequest(key);
    if (!_onboardingState.isAvailableDriveSelected(key)) return;

    if (defaultPath.isEmpty()) {
        qCWarning(lcOnboardingDefaultPathResolver)
                << "No default sync folder available, unselecting drive | driveId:" << key.driveId;
        _onboardingState.unselectAvailableDrive(key);
        _serviceEventBus.notifyGenericError(exitInfo, RequestNum::UTILITY_FINDGOODPATHFORNEWSYNC);
        return;
    }

    PendingSyncConfig config = _onboardingState.pendingSyncConfig(key).value_or(PendingSyncConfig{});
    config.defaultLocalPath = defaultPath;
    if (config.localPath.isEmpty() || config.usesDefaultLocalPath) {
        config.localPath = defaultPath;
        config.usesDefaultLocalPath = true;
    }
    _onboardingState.setPendingSyncConfig(key, config);
}

void OnboardingDefaultPathResolver::finishRequest(const AvailableDriveKey &key) {
    if (_pendingKeys.erase(key) == 0) return;
    updatePendingResolutions();
}

void OnboardingDefaultPathResolver::updatePendingResolutions() {
    // Only a request whose drive is still selected blocks onboarding: the folder of an unselected one is never used.
    const bool pending = std::ranges::any_of(
            _pendingKeys, [this](const AvailableDriveKey &key) { return _onboardingState.isAvailableDriveSelected(key); });
    if (pending == _pendingResolutions) return;
    _pendingResolutions = pending;
    emit pendingResolutionsChanged();
}

} // namespace KDC
