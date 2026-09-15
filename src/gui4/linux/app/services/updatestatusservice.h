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

#include "app/services/commservice.h"

#include <QObject>

#include <cstdint>
#include <functional>
#include <optional>

namespace KDC {

class ParametersStore;

/** Process-wide read-only updater state. Linux exposes detection and release metadata, never installation. */
class UpdateStatusService final : public QObject {
        Q_OBJECT

    public:
        using StateRequest = std::function<void(const CommService::UpdateStateCallback &)>;
        using VersionRequest = std::function<void(const CommService::VersionInfoCallback &)>;

        UpdateStatusService(const CommService &commService, const ParametersStore &parametersStore, QObject *parent = nullptr);
        UpdateStatusService(StateRequest stateRequest, VersionRequest versionRequest, QObject *parent = nullptr);

        void refresh();
        void setState(UpdateState state);

        [[nodiscard]] UpdateState state() const { return _state; }

        [[nodiscard]] const std::optional<VersionInfo> &version() const { return _version; }

        [[nodiscard]] bool available() const;

    signals:
        void changed();
        void stateChanged(KDC::UpdateState state);

    private:
        StateRequest _stateRequest;
        VersionRequest _versionRequest;
        UpdateState _state{UpdateState::Unknown};
        std::optional<VersionInfo> _version;
        uint64_t _generation{0};
};

} // namespace KDC
