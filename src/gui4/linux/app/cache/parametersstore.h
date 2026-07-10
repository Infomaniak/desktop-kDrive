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

#include "libcommon/info/parametersinfo.h"

#include <QLoggingCategory>
#include <QObject>

#include <optional>

Q_DECLARE_LOGGING_CATEGORY(lcParametersStore)

namespace KDC {

/**
 * Process-wide cache for server-owned application parameters.
 *
 * Role: hold the latest confirmed ParametersInfo snapshot fetched from the server bootstrap, plus an optional draft
 * snapshot while an update is awaiting server confirmation. The server remains the persistence source of truth.
 */
class ParametersStore final : public QObject {
        Q_OBJECT
        Q_PROPERTY(bool populated READ populated NOTIFY populatedChanged)
        Q_PROPERTY(bool updatePending READ updatePending NOTIFY updatePendingChanged)

    public:
        explicit ParametersStore(QObject *parent = nullptr);

        [[nodiscard]] bool populated() const;
        [[nodiscard]] bool updatePending() const;

        /**
         * Last server-confirmed parameters snapshot.
         *
         * This is the durable GUI-side source of truth. It changes after bootstrap, refresh, or a successful
         * PARAMETERS_UPDATE response, never when an optimistic update is merely pending.
         */
        [[nodiscard]] std::optional<ParametersInfo> currentParametersInfo() const;

        /**
         * Optimistic parameters snapshot awaiting server confirmation.
         *
         * Present only between beginUpdate() and confirmUpdate()/rejectUpdate(). UI can use it for immediate feedback,
         * but services that require confirmed persistence should keep using currentParametersInfo().
         */
        [[nodiscard]] std::optional<ParametersInfo> draftParametersInfo() const;

        /**
         * Parameters snapshot that should be displayed to users.
         *
         * Returns draftParametersInfo() while an update is pending, otherwise currentParametersInfo(). This may differ
         * from the confirmed server state until PARAMETERS_UPDATE succeeds.
         */
        [[nodiscard]] std::optional<ParametersInfo> effectiveParametersInfo() const;

        void replaceParametersInfo(const ParametersInfo &parametersInfo);

        /**
         * Starts an optimistic update. Does not modify currentParametersInfo().
         *
         * The caller must send PARAMETERS_UPDATE separately and then call confirmUpdate() or rejectUpdate() from the
         * server response handler.
         */
        void beginUpdate(const ParametersInfo &draftParametersInfo);

        /**
         * Commits a server-confirmed update and clears the optimistic draft.
         */
        void confirmUpdate(const ParametersInfo &confirmedParametersInfo);

        /**
         * Discards the optimistic draft after a failed server update. The confirmed snapshot is left untouched.
         */
        void rejectUpdate();
        void clear();

    signals:
        void populatedChanged();
        void updatePendingChanged();
        void parametersInfoChanged();
        void draftParametersInfoChanged();
        void effectiveParametersInfoChanged();

    private:
        std::optional<ParametersInfo> _parametersInfo;
        std::optional<ParametersInfo> _draftParametersInfo;
};

} // namespace KDC
