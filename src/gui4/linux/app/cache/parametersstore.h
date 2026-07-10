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
 * Role: hold the latest server-confirmed ParametersInfo snapshot fetched from bootstrap or a successful
 * PARAMETERS_UPDATE. The server remains the persistence source of truth; screen-level drafts belong to the UI/view
 * model that owns the edit workflow.
 */
class ParametersStore final : public QObject {
        Q_OBJECT
        Q_PROPERTY(bool populated READ populated NOTIFY populatedChanged)

    public:
        explicit ParametersStore(QObject *parent = nullptr);

        [[nodiscard]] bool populated() const;

        /**
         * Last server-confirmed parameters snapshot.
         */
        [[nodiscard]] std::optional<ParametersInfo> parametersInfo() const;

        void replaceParametersInfo(const ParametersInfo &parametersInfo);
        void clear();

    signals:
        void populatedChanged();
        void parametersInfoChanged();

    private:
        std::optional<ParametersInfo> _parametersInfo;
};

} // namespace KDC
