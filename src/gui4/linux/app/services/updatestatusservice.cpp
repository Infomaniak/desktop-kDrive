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

#include "version.h"

#include <QLoggingCategory>
#include <QPointer>

#include <utility>

namespace KDC {

using namespace Qt::StringLiterals;

namespace {
Q_LOGGING_CATEGORY(lcUpdateStatusService, "gui.v4.updatestatusservice", QtInfoMsg)
} // namespace

UpdateStatusService::UpdateStatusService(StateRequest stateRequest, VersionRequest versionRequest, InstallRequest installRequest,
                                         QObject *const parent) :
    QObject(parent),
    _stateRequest(std::move(stateRequest)),
    _versionRequest(std::move(versionRequest)),
    _installRequest(std::move(installRequest)) {}

QString UpdateStatusService::announcedRelease() const {
    if (!_version) {
        return {};
    }

    // A new build of the installed release shares its tag: the build number tells them apart.
    const auto tag = QString::fromStdString(_version->tag);
    const auto installedRelease = u"%1.%2.%3"_s.arg(KDRIVE_VERSION_MAJOR).arg(KDRIVE_VERSION_MINOR).arg(KDRIVE_VERSION_PATCH);
    return tag == installedRelease ? u"%1.%2"_s.arg(tag).arg(_version->buildVersion) : tag;
}

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

// User action: prepares the available update then restarts on it, or restarts on the update already installed.
void UpdateStatusService::installUpdate() {
    if (installable()) {
        _restartWhenReady = true;
        requestInstaller();
    } else if (readyToInstall()) {
        requestInstaller();
    }
}

// Prepares the available update, or restarts on the installed one: the server picks the step from its own state.
void UpdateStatusService::requestInstaller() {
    qCInfo(lcUpdateStatusService) << "Requesting the update installer | state:" << _state;
    _installRequest([self = QPointer(this)](const ExitInfo &result) {
        if (result) {
            return;
        }

        qCWarning(lcUpdateStatusService) << "Update installer request failed | code:" << result.code()
                                         << "/ cause:" << result.cause();
        // A later automatic preparation must not restart the application without the user's request.
        if (self) {
            self->_restartWhenReady = false;
        }
    });
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

    switch (state) {
        case UpdateState::Ready:
            // The user asked for the update before it was installed: restart on it now.
            if (std::exchange(_restartWhenReady, false)) {
                requestInstaller();
            }
            break;
        case UpdateState::Checking:
        case UpdateState::Available:
        case UpdateState::Downloading:
            // A periodic check during the preparation keeps the user's request.
            break;
        default:
            _restartWhenReady = false;
            break;
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
