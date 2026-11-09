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

#include <QLoggingCategory>
#include <QPointer>

#include <utility>

namespace KDC {

Q_LOGGING_CATEGORY(lcUpdateStatusService, "gui.v4.updatestatusservice", QtInfoMsg)

UpdateStatusService::UpdateStatusService(StateRequest stateRequest, VersionRequest versionRequest, InstallRequest installRequest,
                                         QObject *const parent) :
    QObject(parent),
    _stateRequest(std::move(stateRequest)),
    _versionRequest(std::move(versionRequest)),
    _installRequest(std::move(installRequest)) {}

bool UpdateStatusService::available() const {
    // Download and installation errors keep the update available: the user can still download it manually.
    switch (_state) {
        case UpdateState::ManualUpdateAvailable:
        case UpdateState::Available:
        case UpdateState::Downloading:
        case UpdateState::Ready:
        case UpdateState::DownloadError:
        case UpdateState::UpdateError:
            return true;
        default:
            return false;
    }
}

void UpdateStatusService::startInstaller() const {
    if (!readyToInstall()) {
        return;
    }

    // On success the server replaces the AppImage, then asks this client to quit before relaunching.
    qCInfo(lcUpdateStatusService) << "Requesting the update installation";
    _installRequest([](const ExitInfo &result) {
        if (!result) {
            qCWarning(lcUpdateStatusService)
                    << "Update installation request failed | code:" << result.code() << "/ cause:" << result.cause();
        }
    });
}

void UpdateStatusService::refresh() {
    const auto generation = ++_generation;
    _stateRequest([self = QPointer(this), generation](const ExitInfo &result, const UpdateState state) {
        if (!self || self->_generation != generation) {
            return;
        }

        self->setState(result ? state : UpdateState::CheckError);
    });
}

void UpdateStatusService::setState(const UpdateState state) {
    const auto generation = ++_generation;
    _state = state;
    _version.reset();

    emit stateChanged(state);
    emit changed();

    if (!available()) {
        return;
    }

    _versionRequest([self = QPointer(this), generation](const ExitInfo &result, const VersionInfo &version) {
        if (!self || self->_generation != generation) {
            return;
        }

        if (result && !version.tag.empty() && version.buildVersion != 0) {
            self->_version = version;
        }

        emit self->changed();
    });
}

} // namespace KDC
