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

#include "parametersstore.h"

Q_LOGGING_CATEGORY(lcParametersStore, "gui.v4.parametersstore", QtInfoMsg)

namespace KDC {

ParametersStore::ParametersStore(QObject *const parent) :
    QObject(parent) {}

bool ParametersStore::populated() const {
    return _parametersInfo.has_value();
}

std::optional<ParametersInfo> ParametersStore::parametersInfo() const {
    return _parametersInfo;
}

void ParametersStore::replaceParametersInfo(const ParametersInfo &parametersInfo) {
    const bool wasPopulated = populated();
    if (_parametersInfo.has_value() && *_parametersInfo == parametersInfo) {
        return;
    }

    _parametersInfo = parametersInfo;
    qCInfo(lcParametersStore) << "Parameters snapshot updated";
    if (wasPopulated != populated()) {
        emit populatedChanged();
    }
    emit parametersInfoChanged();
}

void ParametersStore::clear() {
    if (const bool wasPopulated = populated(); !wasPopulated) {
        return;
    }

    _parametersInfo.reset();
    qCInfo(lcParametersStore) << "Parameters snapshot cleared";
    emit populatedChanged();
    emit parametersInfoChanged();
}

} // namespace KDC
