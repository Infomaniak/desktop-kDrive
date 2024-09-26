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

#include "libcommon/updater.h"

#include <QObject>
#include <QString>

namespace KDC {

class UpdaterClient : public Updater {
        Q_OBJECT

    public:
        ~UpdaterClient();
        static UpdaterClient *instance();

        // QString version() const override;
        // bool isKDCUpdater() override;
        // bool isSparkleUpdater() override;
        QString statusString() const override;
        bool downloadCompleted() const override;
        bool updateFound() const override;
        void startInstaller() const override;
        UpdateState updateState() const override;
        void unskipUpdate() const;

        void showWindowsUpdaterDialog(const QString &targetVersion, const QString &targetVersionString,
                                      const QString &clientVersion);

    signals:
        void downloadStateChanged();

    private:
        static UpdaterClient *_instance;

        explicit UpdaterClient(QObject *parent = 0);

        friend class AppClient;
};

} // namespace KDC
