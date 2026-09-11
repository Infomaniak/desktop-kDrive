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

#include "parametersservice.h"
#include "updatestatusservice.h"

#include "app/cache/parametersstore.h"

namespace KDC {

// Production constructor that adapts CommService's PARAMETERS_UPDATE request to the injectable update function used by
// ParametersService. Keeping this binding here lets the service serialize mutations without depending on IPC details.
ParametersService::ParametersService(const CommService &commService, ParametersStore &parametersStore, QObject *const parent) :
    ParametersService(
            [&commService](const ParametersInfo &parametersInfo, const UpdateCallback &callback) {
                commService.requestParametersUpdate(parametersInfo, callback);
            },
            parametersStore, parent) {}

// Production constructor that binds updater state and version requests to CommService. The distribution channel comes
// from the latest confirmed parameters, while server-pushed state changes update the same shared service instance.
UpdateStatusService::UpdateStatusService(const CommService &commService, const ParametersStore &parametersStore,
                                         QObject *const parent) :
    UpdateStatusService(
            [&commService](const CommService::UpdateStateCallback &callback) { commService.requestUpdaterState(callback); },
            [&commService, &parametersStore](const CommService::VersionInfoCallback &callback) {
                const auto parametersInfo = parametersStore.parametersInfo();
                const auto channel = parametersInfo ? parametersInfo->distributionChannel() : DistributionChannel::Unknown;
                commService.requestUpdaterVersionInfo(channel, callback);
            },
            parent) {
    (void) connect(&commService, &CommService::updateStateChanged, this, &UpdateStatusService::setState);
}

} // namespace KDC
