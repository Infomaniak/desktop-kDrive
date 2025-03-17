/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
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

#include "utility/types.h"

#include <QTimer>

/**
 * @brief Schedule regular update checks.
 * @ingroup server
 *
 * This class schedules update checks every hour and manage installation steps.
 *
 * This class is still based on Qt. It will be replaced later if Qt is removed from the project.
 */

namespace KDC {

class UpdateManager final : public QObject {
        Q_OBJECT
    public:
        UpdateManager(UpdateManager &) = delete;
        UpdateManager(UpdateManager &&) = delete;
        UpdateManager &operator=(UpdateManager &) = delete;
        UpdateManager &operator=(UpdateManager &&) = delete;

        static std::shared_ptr<UpdateManager> instance();

        void setDistributionChannel(VersionChannel channel);
        [[nodiscard]] const VersionInfo &versionInfo(const VersionChannel channel = VersionChannel::Unknown) const;
        [[nodiscard]] const UpdateState &state() const;

        void startInstaller() const;
        void setQuitCallback(const std::function<void()> &quitCallback) const;

    signals:
        void updateAnnouncement(const QString &title, const QString &msg);
        void requestRestart();
        void updateStateChanged(KDC::UpdateState mewState);
        void showUpdateDialog();

    private slots:
        void slotTimerFired() const;
        void slotUpdateStateChanged(KDC::UpdateState newState);

    private:
        static std::shared_ptr<UpdateManager> _instance;

        explicit UpdateManager(QObject *parent = nullptr);
        void onUpdateStateChanged(UpdateState newState);

        VersionChannel _currentChannel{VersionChannel::Unknown};
        QTimer _updateCheckTimer; /** Timer for the regular update check. */
};

} // namespace KDC
