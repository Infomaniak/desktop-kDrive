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

#include "updatestatusservice.h"

#include <QPointer>

#include <utility>

namespace KDC {

UpdateStatusService::UpdateStatusService(StateRequest stateRequest, VersionRequest versionRequest, QObject *const parent) :
    QObject(parent),
    _stateRequest(std::move(stateRequest)),
    _versionRequest(std::move(versionRequest)) {}

bool UpdateStatusService::available() const {
    // The Linux updater on the server side currently returns only ManualUpdateAvailable.
    return _state == UpdateState::ManualUpdateAvailable || _state == UpdateState::Available || _state == UpdateState::Ready;
}

void UpdateStatusService::refresh() {
    // An explicit refresh also fetches the version again, in case a state change was missed.
    _forceVersionRefresh = true;

    const auto generation = ++_generation;
    _stateRequest([self = QPointer(this), generation](const ExitInfo &result, const UpdateState state) {
        if (!self || self->_generation != generation) {
            return;
        }

        self->setState(result ? state : UpdateState::CheckError);
    });
}

void UpdateStatusService::setState(const UpdateState state) {
    ++_generation;
    const bool wasAvailable = available();
    _state = state;

    // The server only changes the announced version during a new check, which always goes through Checking. Moving
    // between available states (Available, Downloading, Ready, errors) therefore keeps the same version.
    if (const bool forceRefresh = std::exchange(_forceVersionRefresh, false); !wasAvailable || !available() || forceRefresh) {
        ++_versionGeneration;
        _version.reset();
        _versionPending = false;
    }

    emit stateChanged(state);
    emit changed();

    if (available() && !_version && !_versionPending) {
        requestVersion();
    }
}

void UpdateStatusService::requestVersion() {
    const auto generation = _versionGeneration;
    _versionPending = true;
    _versionRequest([self = QPointer(this), generation](const ExitInfo &result, const VersionInfo &version) {
        if (!self || self->_versionGeneration != generation) {
            return;
        }

        // A failed request leaves the version empty: the next available state asks again.
        self->_versionPending = false;
        if (result && !version.tag.empty() && version.buildVersion != 0) {
            self->_version = version;
        }

        emit self->changed();
    });
}

} // namespace KDC
