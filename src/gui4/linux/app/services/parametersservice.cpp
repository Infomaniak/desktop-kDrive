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
#include <QPointer>

#include <utility>

namespace KDC {

namespace {
Q_LOGGING_CATEGORY(lcParametersService, "gui.v4.parametersservice", QtInfoMsg)
} // namespace

ParametersService::ParametersService(UpdateRequest request, ParametersStore &parametersStore, QObject *const parent) :
    QObject(parent),
    _request(std::move(request)),
    _parametersStore(parametersStore) {}

void ParametersService::updateParameters(const ParametersMutation &mutation, const UpdateCallback &callback) {
    if (!mutation) {
        if (callback) {
            callback({ExitCode::LogicError, ExitCause::InvalidArgument});
        }
        return;
    }

    _updates.push_back({.mutation = mutation, .callback = callback});
    startNextUpdate();
}

// Serialize full-snapshot writes. Queued mutations must start from the previous confirmed result, not the snapshot
// that existed when the user clicked. Keep the queue locked through notifications and callbacks, which may enqueue work.
void ParametersService::startNextUpdate() {
    if (_updating || _updates.empty()) {
        return;
    }

    _updating = true;
    const auto update = _updates.front();
    auto parametersInfo = _parametersStore.parametersInfo();
    if (!parametersInfo) {
        _updates.pop_front();

        const QPointer self(this);
        if (update.callback) {
            update.callback({ExitCode::DataError, ExitCause::NotFound});
        }

        if (!self) {
            return;
        }

        _updating = false;
        startNextUpdate();
        return;
    }

    const auto confirmedParametersInfo = *parametersInfo;
    update.mutation(*parametersInfo);

    const auto finishUpdate = [self = QPointer(this), parametersInfo = *parametersInfo, update](const ExitInfo &result) {
        if (!self) {
            return;
        }

        self->_updates.pop_front();
        if (result) {
            self->_parametersStore.replaceParametersInfo(parametersInfo);
        } else {
            qCWarning(lcParametersService) << "Parameters update rejected:" << QString::fromStdString(toString(result));
        }

        if (!self) {
            return;
        }

        if (update.callback) {
            update.callback(result);
        }

        if (!self) {
            return;
        }

        self->_updating = false;
        self->startNextUpdate();
    };

    if (*parametersInfo == confirmedParametersInfo) {
        finishUpdate(ExitInfo{ExitCode::Ok});
        return;
    }

    _request(*parametersInfo, finishUpdate);
}

} // namespace KDC
