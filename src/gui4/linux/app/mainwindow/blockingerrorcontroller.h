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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */


#pragma once

#include "app/errors/syncerrorkind.h"

#include <QColor>
#include <QObject>
#include <QString>

#include <cstdint>
#include <optional>

namespace KDC {

class AppRouter;
class MainSelectionStore;
class SyncErrorsProjection;
class SyncService;

/**
 * QML-facing state of the blocking error screen of the selected synchronization (drive asleep, waking up, not renewed,
 * in maintenance, access denied, or user logged out).
 *
 * Only the selected synchronization is blocked: the screen replaces the page content through AppRouter, and returns to
 * Home once the error is gone or another synchronization is selected. Texts are translated in QML from `kind`.
 */
class BlockingErrorController final : public QObject {
        Q_OBJECT
        Q_PROPERTY(bool active READ active NOTIFY changed)
        Q_PROPERTY(KDC::BlockingErrors::BlockingErrorKind kind READ kind NOTIFY changed)
        Q_PROPERTY(QString driveName READ driveName NOTIFY changed)
        Q_PROPERTY(QColor driveColor READ driveColor NOTIFY changed)
        Q_PROPERTY(bool admin READ admin NOTIFY changed)
        Q_PROPERTY(bool actionPending READ actionPending NOTIFY actionPendingChanged)

    public:
        BlockingErrorController(const SyncErrorsProjection &projection, const MainSelectionStore &selectionStore,
                                SyncService &syncService, AppRouter &appRouter, QObject *parent = nullptr);

        [[nodiscard]] bool active() const { return _blockingError.has_value(); }
        // Only meaningful while active.
        [[nodiscard]] BlockingErrorKind kind() const { return _blockingError.value_or(BlockingErrorKind::EnumEnd); }
        [[nodiscard]] QString driveName() const;
        [[nodiscard]] QColor driveColor() const;
        [[nodiscard]] bool admin() const;
        [[nodiscard]] bool actionPending() const;

        Q_INVOKABLE void triggerAction();

    signals:
        void changed();
        void actionPendingChanged();
        void loginRequested();

    private:
        void refresh();
        void keepBlockingErrorTab();
        void restartCurrentSync();

        const SyncErrorsProjection &_projection;
        const MainSelectionStore &_selectionStore;
        SyncService &_syncService;
        AppRouter &_appRouter;
        std::optional<BlockingErrorKind> _blockingError;
};

} // namespace KDC
