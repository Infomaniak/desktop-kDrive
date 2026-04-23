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

#include "serviceeventbus.h"

#include "libcommon/utility/types.h"

#include <QLoggingCategory>

namespace KDC {

Q_LOGGING_CATEGORY(lcServiceEventBus, "gui.v4.serviceeventbus", QtInfoMsg)

ServiceEventBus::ServiceEventBus(QObject *const parent) :
    QObject(parent) {}

void ServiceEventBus::notifyGenericError(const ExitInfo &exitInfo, const RequestNum requestNum) {
    qCWarning(lcServiceEventBus) << "Generic service error | request:" << toInt(requestNum) << "/ code:" << exitInfo.code()
                                 << "/ cause:" << exitInfo.cause();
    // TODO(gui4/linux): capture this error in Sentry once Sentry is integrated in the Linux v4 app.
    emit genericErrorOccurred();
}

} // namespace KDC
