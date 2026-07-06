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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "driveselectioncontroller.h"

namespace KDC {

DriveSelectionController::DriveSelectionController(AppCache &cache, OnboardingState &onboardingState, UserService &userService,
                                                   OnboardingFlowController &flowController, QObject *const parent) :
    QObject(parent),
    _cache(cache),
    _onboardingState(onboardingState),
    _userService(userService),
    _flowController(flowController),
    _drivesModel(cache, onboardingState, this) {
    (void) connect(&_cache, &AppCache::usersChanged, this, &DriveSelectionController::userChanged);
    (void) connect(&_onboardingState, &OnboardingState::selectedUserDbIdChanged, this, [this] {
        setLoadFailed(false);
        emit userChanged();
        emit loadingChanged();
        emit emptyChanged();
    });
    (void) connect(&_drivesModel, &QAbstractItemModel::modelReset, this, &DriveSelectionController::emptyChanged);
    (void) connect(&_drivesModel, &AvailableDrivesModel::selectedCountChanged, this, [this] {
        emit selectedCountChanged();
        emit canContinueChanged();
        emit canOpenAdvancedSettingsChanged();
    });
    (void) connect(&_drivesModel, &AvailableDrivesModel::configuredCountChanged, this, [this] {
        emit configuredCountChanged();
        emit canContinueChanged();
    });
    (void) connect(&_userService, &UserService::availableDrivesLoadingChanged, this, [this](const UserDbId userDbId) {
        if (userDbId == selectedUserDbId()) {
            emit loadingChanged();
            emit emptyChanged();
        }
    });
    (void) connect(&_userService, &UserService::availableDrivesLoaded, this, [this](const UserDbId userDbId) {
        if (userDbId == selectedUserDbId()) {
            setLoadFailed(false);
        }
    });
    (void) connect(&_userService, &UserService::availableDrivesLoadFailed, this, [this](const UserDbId userDbId) {
        if (userDbId == selectedUserDbId()) {
            setLoadFailed(true);
        }
    });
}

bool DriveSelectionController::loading() const {
    const auto userDbId = selectedUserDbId();
    return userDbId != 0 && _userService.isLoadAvailableDrivesPending(static_cast<qint64>(userDbId));
}

bool DriveSelectionController::empty() const {
    return !loading() && !_loadFailed && _drivesModel.rowCount() == 0;
}

qint32 DriveSelectionController::selectedCount() const {
    return _drivesModel.selectedCount();
}

qint32 DriveSelectionController::configuredCount() const {
    return _drivesModel.configuredCount();
}

bool DriveSelectionController::canContinue() const {
    return selectedCount() > 0 || configuredCount() > 0;
}

bool DriveSelectionController::canOpenAdvancedSettings() const {
    return selectedCount() > 0;
}

QString DriveSelectionController::userName() const {
    if (const auto user = _cache.userDisplayInfo(selectedUserDbId())) {
        return user->name();
    }
    return {};
}

QString DriveSelectionController::userEmail() const {
    if (const auto user = _cache.userDisplayInfo(selectedUserDbId())) {
        return user->email();
    }
    return {};
}

QString DriveSelectionController::userAvatarSource() const {
    if (const auto user = _cache.userDisplayInfo(selectedUserDbId())) {
        if (!user->avatarSource().isEmpty()) {
            return user->avatarSource();
        }
        return user->avatarUrl();
    }
    return {};
}

void DriveSelectionController::reload() {
    const auto userDbId = selectedUserDbId();
    if (userDbId == 0 || loading()) {
        return;
    }

    setLoadFailed(false);
    _userService.loadAvailableDrives(static_cast<qint64>(userDbId));
}

void DriveSelectionController::requestAdvancedSettings() {
    _flowController.requestAdvancedSettings();
}

void DriveSelectionController::continueOnboarding() {
    if (!canContinue()) {
        return;
    }

    _flowController.requestDriveSelectionContinue();
}

void DriveSelectionController::openDriveOffers() {
    _flowController.requestDriveOffers();
}

void DriveSelectionController::startForFree() {
    _flowController.requestFreeDriveOrder();
}

UserDbId DriveSelectionController::selectedUserDbId() const {
    return _onboardingState.typedSelectedUserDbId();
}

void DriveSelectionController::setLoadFailed(const bool loadFailed) {
    if (_loadFailed == loadFailed) {
        return;
    }

    _loadFailed = loadFailed;
    emit loadFailedChanged();
    emit emptyChanged();
}

} // namespace KDC
