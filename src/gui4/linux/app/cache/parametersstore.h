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

#include "libcommon/data/parameters.h"

#include <QLoggingCategory>
#include <QObject>

#include <optional>

Q_DECLARE_LOGGING_CATEGORY(lcParametersStore)

namespace KDC {

/**
 * Process-wide cache for server-owned application parameters.
 *
 * Role: hold the latest server-confirmed Parameters snapshot fetched from bootstrap or a successful
 * PARAMETERS_UPDATE. The server remains the persistence source of truth; screen-level drafts belong to the UI/view
 * model that owns the edit workflow.
 */
class ParametersStore final : public QObject {
        Q_OBJECT

    public:
        explicit ParametersStore(QObject *parent = nullptr);

        /**
         * Last server-confirmed parameters snapshot.
         */
        [[nodiscard]] std::optional<Parameters> parameters() const;

        void replaceParameters(const Parameters &parameters);
        void clear();

    signals:
        void parametersChanged();

    private:
        std::optional<Parameters> _parameters;
};

} // namespace KDC
