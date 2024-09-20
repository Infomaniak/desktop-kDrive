
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

#pragma once
#include <QObject>
#include <QTimer>

namespace KDC {

/**
 * @brief Schedule update checks every couple of hours if the client runs.
 * @ingroup server
 *
 * This class schedules regular update checks.
 */

class UpdaterScheduler final : public QObject {
        Q_OBJECT
    public:
        explicit UpdaterScheduler(QObject *parent);

    signals:
        void updaterAnnouncement(const QString &title, const QString &msg);
        void requestRestart();

    private slots:
        void slotTimerFired();

    private:
        QTimer _updateCheckTimer; /** Timer for the regular update check. */
};

} // namespace KDC
