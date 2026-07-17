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

#include "app/cache/parametersstore.h"

#include <QLoggingCategory>
#include <QString>

namespace KDC {

namespace {
Q_LOGGING_CATEGORY(lcParametersService, "gui.v4.parametersservice", QtInfoMsg)
} // namespace

ParametersService::ParametersService(CommService &commService, ParametersStore &parametersStore, QObject *const parent) :
    QObject(parent),
    _commService(commService),
    _parametersStore(parametersStore) {}

void ParametersService::updateParameters(const ParametersMutation &mutation, const UpdateCallback &callback) const {
    if (!mutation) {
        qCWarning(lcParametersService) << "Parameters update ignored because no mutation was provided";
        if (callback) {
            callback({ExitCode::LogicError, ExitCause::InvalidArgument});
        }
        return;
    }

    const auto currentParametersInfo = _parametersStore.parametersInfo();
    if (!currentParametersInfo.has_value()) {
        qCWarning(lcParametersService) << "Parameters update ignored because server parameters are not loaded yet";
        if (callback) {
            callback({ExitCode::DataError, ExitCause::NotFound});
        }
        return;
    }

    ParametersInfo updatedParametersInfo = *currentParametersInfo;
    mutation(updatedParametersInfo);
    _commService.requestParametersUpdate(
            updatedParametersInfo, [this, updatedParametersInfo, callback](const ExitInfo &exitInfo) {
                if (!exitInfo) {
                    qCWarning(lcParametersService)
                            << "Parameters update rejected by server | ExitInfo:" << QString::fromStdString(toString(exitInfo));
                    if (callback) {
                        callback(exitInfo);
                    }
                    return;
                }

                qCInfo(lcParametersService) << "Parameters update confirmed by server";
                _parametersStore.replaceParametersInfo(updatedParametersInfo);
                if (callback) {
                    callback(exitInfo);
                }
            });
}

} // namespace KDC
