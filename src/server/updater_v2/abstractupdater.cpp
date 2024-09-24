
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

#include "abstractupdater.h"

#include "libcommon/utility/utility.h"

namespace KDC {

AbstractUpdater::AbstractUpdater() {
    const std::function callback = [this] { onAppVersionReceived(); };
    _updateChecker->setCallback(callback);
}

ExitCode AbstractUpdater::checkUpdateAvailable(UniqueId* id) {
    setState(UpdateStateV2::Checking);
    return _updateChecker->checkUpdateAvailable(id);
}

void AbstractUpdater::setStateChangeCallback(const std::function<void(UpdateStateV2)>& stateChangeCallback) {
    LOG_INFO(Log::instance()->getLogger(), "Set state change callback");
    _stateChangeCallback = stateChangeCallback;
}

void AbstractUpdater::onAppVersionReceived() {
    if (!_updateChecker->versionInfo().isValid()) {
        setState(UpdateStateV2::Error);
        LOG_WARN(Log::instance()->getLogger(), "Error while retrieving latest app version");
    }

    const bool available =
            CommonUtility::isVersionLower(CommonUtility::currentVersion(), _updateChecker->versionInfo().fullVersion());
    setState(available ? UpdateStateV2::Available : UpdateStateV2::UpToDate);
    if (available)
        LOG_INFO(Log::instance()->getLogger(), "New app version available");
    else
        LOG_INFO(Log::instance()->getLogger(), "App version is up to date");
}

void AbstractUpdater::setState(const UpdateStateV2 newState) {
    if (_state != newState) {
        LOG_DEBUG(Log::instance()->getLogger(), "Update state changed to: " << newState);
        _state = newState;
        if (_stateChangeCallback) _stateChangeCallback(_state);
    }
}

} // namespace KDC
