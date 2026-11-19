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

#pragma once

#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QWindow>

#include <vector>

namespace KDC {

class UpdateStatusService;

/**
 * Restarts the application on an update the server installed by itself, once doing so cannot interrupt the user.
 *
 * The server installs automatic updates without restarting (state `Ready`). The restart is requested with the
 * installer request when no watched window is visible, after a quiet period. Running synchronizations do not delay it:
 * they resume after the restart, and a drive that is always busy must still receive the update. The next launch uses
 * the installed update anyway, so waiting never loses it.
 */
class UpdateRestartCoordinator final : public QObject {
        Q_OBJECT

    public:
        explicit UpdateRestartCoordinator(UpdateStatusService &updateStatusService, QObject *parent = nullptr);

        /** The restart waits while `window` is visible. */
        void watchWindow(QWindow *window);

    private:
        void scheduleRestart();
        void restartIfQuiet();
        [[nodiscard]] bool canRestart() const;
        [[nodiscard]] bool anyWindowVisible() const;

        UpdateStatusService &_updateStatusService;
        std::vector<QPointer<QWindow>> _windows;
        QTimer _quietTimer;
        bool _restartRequested{false};
};

} // namespace KDC
