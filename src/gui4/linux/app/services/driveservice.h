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

#include "app/cache/appcache.h"
#include "app/services/commservice.h"

#include <QObject>
#include <QString>

#include <cstdint>

namespace KDC {

/**
 * High-level drive-oriented facade.
 *
 * QML should use the Q_INVOKABLE methods. updateDrive() remains C++-facing until
 * a stable QML edit contract for DriveInfo is introduced.
 */
class DriveService : public QObject {
        Q_OBJECT
        Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
        Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
        Q_PROPERTY(qint64 activeDriveDbId READ activeDriveDbId NOTIFY activeDriveDbIdChanged)

    public:
        explicit DriveService(CommService &commService, AppCache &appCache, QObject *parent = nullptr);

        bool loading() const { return _loading; }
        const QString &lastError() const { return _lastError; }
        qint64 activeDriveDbId() const { return _appCache.selectedDriveDbId(); }

        Q_INVOKABLE void loadDrives();
        Q_INVOKABLE void deleteDrive(qint64 driveDbId);
        Q_INVOKABLE void setActiveDrive(qint64 driveDbId);

        void updateDrive(const DriveInfo &driveInfo);

    signals:
        void loadingChanged();
        void lastErrorChanged();
        void activeDriveDbIdChanged();

    private:
        void beginRequest();
        void endRequest();
        void setLoading(bool loading);
        void setLastError(const QString &error);

        CommService &_commService;
        AppCache &_appCache;
        Count _pendingRequestCount{0};
        bool _loading{false};
        QString _lastError;
};

} // namespace KDC
