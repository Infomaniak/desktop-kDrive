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

#include "app/cache/cachetypes.h"
#include "app/cache/mainselectionstore.h"

#include <QObject>

#include <optional>

namespace KDC {

/**
 * QML-facing adapter for the current main-flow sync context.
 *
 * Role: expose MainSelectionStore::currentSyncContext() as stable scalar QML properties.
 * Non-role: own cache data, perform protocol requests, or duplicate list-model state.
 */
class CurrentSyncModel : public QObject {
        Q_OBJECT
        Q_PROPERTY(bool hasSync READ hasSync NOTIFY contextChanged)
        Q_PROPERTY(qint64 syncDbId READ syncDbId NOTIFY contextChanged)
        Q_PROPERTY(qint64 driveDbId READ driveDbId NOTIFY contextChanged)
        Q_PROPERTY(qint64 driveId READ driveId NOTIFY contextChanged)
        Q_PROPERTY(QString driveName READ driveName NOTIFY contextChanged)
        Q_PROPERTY(qint64 accountDbId READ accountDbId NOTIFY contextChanged)
        Q_PROPERTY(qint64 accountId READ accountId NOTIFY contextChanged)
        Q_PROPERTY(QString accountName READ accountName NOTIFY contextChanged)
        Q_PROPERTY(qint64 userDbId READ userDbId NOTIFY contextChanged)
        Q_PROPERTY(qint64 userId READ userId NOTIFY contextChanged)
        Q_PROPERTY(QString userName READ userName NOTIFY contextChanged)
        Q_PROPERTY(QString userEmail READ userEmail NOTIFY contextChanged)
        Q_PROPERTY(QString userAvatarSource READ userAvatarSource NOTIFY contextChanged)
        Q_PROPERTY(QString localPath READ localPath NOTIFY contextChanged)
        Q_PROPERTY(QString targetPath READ targetPath NOTIFY contextChanged)
        Q_PROPERTY(QString targetNodeId READ targetNodeId NOTIFY contextChanged)
        Q_PROPERTY(bool supportVfs READ supportVfs NOTIFY contextChanged)
        Q_PROPERTY(qint32 virtualFileMode READ virtualFileMode NOTIFY contextChanged)
        Q_PROPERTY(QString navigationPaneClsid READ navigationPaneClsid NOTIFY contextChanged)
        Q_PROPERTY(qint64 errorCount READ errorCount NOTIFY contextChanged)
        Q_PROPERTY(bool hasError READ hasError NOTIFY contextChanged)
        Q_PROPERTY(qint64 latestErrorDbId READ latestErrorDbId NOTIFY contextChanged)
        Q_PROPERTY(qint64 latestErrorTime READ latestErrorTime NOTIFY contextChanged)
        Q_PROPERTY(qint32 latestErrorLevel READ latestErrorLevel NOTIFY contextChanged)
        Q_PROPERTY(qint32 latestErrorExitCode READ latestErrorExitCode NOTIFY contextChanged)
        Q_PROPERTY(QString latestErrorPath READ latestErrorPath NOTIFY contextChanged)

    public:
        explicit CurrentSyncModel(MainSelectionStore &mainSelectionStore, QObject *parent = nullptr);

        [[nodiscard]] bool hasSync() const;
        [[nodiscard]] qint64 syncDbId() const;
        [[nodiscard]] qint64 driveDbId() const;
        [[nodiscard]] qint64 driveId() const;
        [[nodiscard]] QString driveName() const;
        [[nodiscard]] qint64 accountDbId() const;
        [[nodiscard]] qint64 accountId() const;
        [[nodiscard]] QString accountName() const;
        [[nodiscard]] qint64 userDbId() const;
        [[nodiscard]] qint64 userId() const;
        [[nodiscard]] QString userName() const;
        [[nodiscard]] QString userEmail() const;
        [[nodiscard]] QString userAvatarSource() const;
        [[nodiscard]] QString localPath() const;
        [[nodiscard]] QString targetPath() const;
        [[nodiscard]] QString targetNodeId() const;
        [[nodiscard]] bool supportVfs() const;
        [[nodiscard]] qint32 virtualFileMode() const;
        [[nodiscard]] QString navigationPaneClsid() const;
        [[nodiscard]] qint64 errorCount() const;
        [[nodiscard]] bool hasError() const;
        [[nodiscard]] qint64 latestErrorDbId() const;
        [[nodiscard]] qint64 latestErrorTime() const;
        [[nodiscard]] qint32 latestErrorLevel() const;
        [[nodiscard]] qint32 latestErrorExitCode() const;
        [[nodiscard]] QString latestErrorPath() const;

    signals:
        void contextChanged();

    private:
        void refreshContext();

        MainSelectionStore &_mainSelectionStore;
        std::optional<SyncContext> _context;
};

} // namespace KDC
