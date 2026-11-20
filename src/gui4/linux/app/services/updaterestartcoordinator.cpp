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

#include "updaterestartcoordinator.h"

#include "app/appconstants.h"
#include "app/services/updatestatusservice.h"

#include <QLoggingCategory>

#include <algorithm>

namespace KDC {

namespace {
Q_LOGGING_CATEGORY(lcUpdateRestartCoordinator, "gui.v4.updaterestartcoordinator", QtInfoMsg)
} // namespace

UpdateRestartCoordinator::UpdateRestartCoordinator(UpdateStatusService &updateStatusService, QObject *const parent) :
    QObject(parent),
    _updateStatusService(updateStatusService) {
    _quietTimer.setSingleShot(true);
    _quietTimer.setInterval(AppConstants::Updater::restartQuietDelayMs);
    (void) connect(&_quietTimer, &QTimer::timeout, this, &UpdateRestartCoordinator::restartIfQuiet);

    (void) connect(&_updateStatusService, &UpdateStatusService::stateChanged, this, [this] {
        // A new installation, for instance after a check that left Ready, can be restarted on again.
        if (!_updateStatusService.installed()) {
            _restartRequested = false;
        }

        scheduleRestart();
    });
}

void UpdateRestartCoordinator::watchWindow(QWindow *const window) {
    if (window == nullptr) {
        return;
    }

    const bool alreadyWatched =
            std::ranges::any_of(_windows, [window](const QPointer<QWindow> &watched) { return watched == window; });
    if (alreadyWatched) {
        return;
    }

    _windows.emplace_back(window);
    (void) connect(window, &QWindow::visibleChanged, this, &UpdateRestartCoordinator::scheduleRestart);
    (void) connect(window, &QObject::destroyed, this, &UpdateRestartCoordinator::scheduleRestart);
    scheduleRestart();
}

// Starts the quiet period when every condition holds, and cancels it as soon as one no longer does.
void UpdateRestartCoordinator::scheduleRestart() {
    if (!shouldRequestRestart()) {
        _quietTimer.stop();
        return;
    }

    if (!_quietTimer.isActive()) {
        _quietTimer.start();
    }
}

void UpdateRestartCoordinator::restartIfQuiet() {
    if (!shouldRequestRestart()) {
        return;
    }

    qCInfo(lcUpdateRestartCoordinator) << "No window open: restarting on the installed update";
    _restartRequested = true;
    _updateStatusService.restartOnInstalledUpdate();
}

bool UpdateRestartCoordinator::shouldRequestRestart() const {
    return _updateStatusService.installed() && !_restartRequested && !anyWindowVisible();
}

bool UpdateRestartCoordinator::anyWindowVisible() const {
    return std::ranges::any_of(_windows, [](const QPointer<QWindow> &window) { return window && window->isVisible(); });
}

} // namespace KDC
