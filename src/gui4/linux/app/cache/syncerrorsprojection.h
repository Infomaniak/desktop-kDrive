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
#include "libcommon/data/error.h"

#include <QObject>
#include <QTimer>

#include <optional>
#include <unordered_map>
#include <vector>

namespace KDC {

class AppCache;
class MainSelectionStore;

struct ClassifiedSyncError {
        Error error;
        SyncErrorKind kind{SyncErrorKind::Unknown};
        SyncErrorCategory category{SyncErrorCategory::SystemAndPermissions};

        friend bool operator==(const ClassifiedSyncError &lhs, const ClassifiedSyncError &rhs) = default;
};

/**
 * Single source of the classified sync errors, shared by every error consumer.
 *
 * Each error is classified and remembered by its database id, and logged once unless its kind changes. Cache signals are
 * coalesced into one refresh within 150 ms (the server replaces a known error with a removed then added signal pair), and
 * each signal is emitted only when its value changed.
 */
class SyncErrorsProjection final : public QObject {
        Q_OBJECT

    public:
        SyncErrorsProjection(const AppCache &cache, const MainSelectionStore &selectionStore, QObject *parent = nullptr);

        // Errors of the selected synchronization, oldest first.
        [[nodiscard]] const std::vector<ClassifiedSyncError> &currentErrors() const { return _currentErrors; }
        // User-resolvable version conflicts of the selected synchronization, sorted by database id.
        [[nodiscard]] std::vector<ErrorDbId> currentConflictIds() const;
        [[nodiscard]] int32_t currentErrorCount() const { return static_cast<int32_t>(_currentErrors.size()); }
        [[nodiscard]] std::optional<BlockingErrorKind> currentBlockingError() const { return _currentBlockingError; }

        // Whether an error of any synchronization turns the system tray icon to its error state.
        [[nodiscard]] bool hasTrayRelevantError() const { return _hasTrayRelevantError; }
        [[nodiscard]] bool isUpdateRequired() const { return _updateRequired; }

    signals:
        void currentErrorsChanged();
        void currentBlockingErrorChanged();
        void trayRelevanceChanged();
        void updateRequiredChanged();

    private:
        void scheduleRefresh();
        void refresh();
        void refreshClassifications(const std::vector<Error> &syncErrors);
        void refreshCurrentErrors();
        void refreshTrayRelevance();
        void refreshUpdateRequired(const std::vector<Error> &syncErrors);

        const AppCache &_cache;
        const MainSelectionStore &_selectionStore;
        QTimer _refreshTimer;

        std::unordered_map<ErrorDbId, ClassifiedSyncError> _classifiedByDbId;
        std::vector<ClassifiedSyncError> _currentErrors;
        std::optional<BlockingErrorKind> _currentBlockingError;
        bool _hasTrayRelevantError{false};
        bool _updateRequired{false};
};

} // namespace KDC
