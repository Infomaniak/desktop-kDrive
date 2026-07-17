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
#include "libcommon/info/parametersinfo.h"

#include <QObject>

#include <functional>

namespace KDC {

class ParametersStore;

/**
 * High-level facade for server-owned application parameter updates.
 *
 * Role: start from the confirmed ParametersStore snapshot, send a full PARAMETERS_UPDATE payload, and publish the new
 * snapshot only after the server confirms it. UI-specific drafts are owned by the relevant view model/screen.
 */
class ParametersService final : public QObject {
        Q_OBJECT

    public:
        using ParametersMutation = std::function<void(ParametersInfo &)>;
        using UpdateCallback = CommService::VoidCallback;

        explicit ParametersService(CommService &commService, ParametersStore &parametersStore, QObject *parent = nullptr);

        void updateParameters(const ParametersMutation &mutation, const UpdateCallback &callback) const;

    private:
        CommService &_commService;
        ParametersStore &_parametersStore;
};

} // namespace KDC
