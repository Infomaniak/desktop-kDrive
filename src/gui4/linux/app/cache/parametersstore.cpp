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

bool ParametersStore::updatePending() const {
    return _draftParametersInfo.has_value();
}

std::optional<ParametersInfo> ParametersStore::currentParametersInfo() const {
    return _parametersInfo;
}

std::optional<ParametersInfo> ParametersStore::draftParametersInfo() const {
    return _draftParametersInfo;
}

std::optional<ParametersInfo> ParametersStore::effectiveParametersInfo() const {
    if (_draftParametersInfo.has_value()) {
        return _draftParametersInfo;
    }
    return _parametersInfo;
}

void ParametersStore::replaceParametersInfo(const ParametersInfo &parametersInfo) {
    const bool wasPopulated = populated();
    const bool hadPendingUpdate = updatePending();
    if (_parametersInfo.has_value() && *_parametersInfo == parametersInfo && !hadPendingUpdate) {
        return;
    }

    _parametersInfo = parametersInfo;
    _draftParametersInfo.reset();
    qCInfo(lcParametersStore) << "Parameters snapshot updated";
    if (wasPopulated != populated()) {
        emit populatedChanged();
    }
    if (hadPendingUpdate) {
        emit updatePendingChanged();
        emit draftParametersInfoChanged();
    }
    emit parametersInfoChanged();
    emit effectiveParametersInfoChanged();
}

void ParametersStore::beginUpdate(const ParametersInfo &draftParametersInfo) {
    const bool hadPendingUpdate = updatePending();
    if (_draftParametersInfo.has_value() && *_draftParametersInfo == draftParametersInfo) {
        return;
    }

    _draftParametersInfo = draftParametersInfo;
    qCInfo(lcParametersStore) << "Parameters draft update started";
    if (hadPendingUpdate != updatePending()) {
        emit updatePendingChanged();
    }
    emit draftParametersInfoChanged();
    emit effectiveParametersInfoChanged();
}

void ParametersStore::confirmUpdate(const ParametersInfo &confirmedParametersInfo) {
    const bool wasPopulated = populated();
    const bool hadPendingUpdate = updatePending();
    const bool confirmedChanged = !_parametersInfo.has_value() || *_parametersInfo != confirmedParametersInfo;

    _parametersInfo = confirmedParametersInfo;
    _draftParametersInfo.reset();
    qCInfo(lcParametersStore) << "Parameters draft update confirmed";

    if (wasPopulated != populated()) {
        emit populatedChanged();
    }
    if (hadPendingUpdate) {
        emit updatePendingChanged();
        emit draftParametersInfoChanged();
    }
    if (confirmedChanged) {
        emit parametersInfoChanged();
    }
    emit effectiveParametersInfoChanged();
}

void ParametersStore::rejectUpdate() {
    if (!_draftParametersInfo.has_value()) {
        return;
    }

    _draftParametersInfo.reset();
    qCInfo(lcParametersStore) << "Parameters draft update rejected";
    emit updatePendingChanged();
    emit draftParametersInfoChanged();
    emit effectiveParametersInfoChanged();
}

void ParametersStore::clear() {
    const bool wasPopulated = populated();
    const bool hadPendingUpdate = updatePending();
    if (!wasPopulated && !hadPendingUpdate) {
        return;
    }

    _parametersInfo.reset();
    _draftParametersInfo.reset();
    qCInfo(lcParametersStore) << "Parameters snapshot cleared";
    if (wasPopulated) {
        emit populatedChanged();
        emit parametersInfoChanged();
    }
    if (hadPendingUpdate) {
        emit updatePendingChanged();
        emit draftParametersInfoChanged();
    }
    emit effectiveParametersInfoChanged();
}

} // namespace KDC
