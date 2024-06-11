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
#include <QString>

namespace KDC {

class UpdaterClient : public QObject {
        Q_OBJECT

    public:
        ~UpdaterClient();
        static UpdaterClient *instance();

        QString version() const;
        bool isKDCUpdater();
        bool isSparkleUpdater();
        QString statusString() const;
        bool downloadCompleted() const;
        bool updateFound() const;
        void startInstaller() const;

        void showWindowsUpdaterDialog(const QString &targetVersion, const QString &targetVersionString,
                                      const QString &clientVersion);

    signals:
        void downloadStateChanged();

    private:
        static UpdaterClient *_instance;

        explicit UpdaterClient(QObject *parent = 0);

        friend class AppClient;
};

}  // namespace KDC
