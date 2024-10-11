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

#include "abstractupdater.h"
#include "utility/types.h"

#include <QObject>
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
        explicit UpdateManager(QObject *parent);

        void setDistributionChannel(const DistributionChannel channel) const {
            _updater->checkUpdateAvailable(channel);
        } // TODO : write to DB
        [[nodiscard]] const VersionInfo &versionInfo() const { return _updater->versionInfo(); }
        [[nodiscard]] const UpdateStateV2 &state() const { return _updater->state(); }

        void startInstaller() const;
        void skipVersion(const std::string &skippedVersion) const;

        void setQuitCallback(const std::function<void()> &quitCallback) const { _updater->setQuitCallback(quitCallback); }

    signals:
        void updateAnnouncement(const QString &title, const QString &msg);
        void requestRestart();
        void updateStateChanged(KDC::UpdateStateV2 mewState);
        void showUpdateDialog();

    private slots:
        void slotTimerFired() const;
        void slotUpdateStateChanged(KDC::UpdateStateV2 newState);

    private:
        /**
         * @brief Create adequat updater according to OS.
         */
        void createUpdater();

        void onUpdateStateChange(UpdateStateV2 newState);

        [[nodiscard]] DistributionChannel readDistributionChannelFromDb() const;

        std::unique_ptr<AbstractUpdater> _updater;

        QTimer _updateCheckTimer; /** Timer for the regular update check. */
};

} // namespace KDC
