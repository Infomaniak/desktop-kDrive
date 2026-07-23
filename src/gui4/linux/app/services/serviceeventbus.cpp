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

#include "app/services/sentryservice.h"
#include "libcommon/utility/types.h"

#include <QLoggingCategory>
#include <QString>

namespace KDC {

Q_LOGGING_CATEGORY(lcServiceEventBus, "gui.v4.serviceeventbus", QtInfoMsg)

ServiceEventBus::ServiceEventBus(QObject *const parent) :
    QObject(parent) {}

void ServiceEventBus::notifyGenericError(const ExitInfo &exitInfo, const RequestNum requestNum) {
    qCWarning(lcServiceEventBus) << "Generic service error | request:" << toInt(requestNum) << "/ code:" << exitInfo.code()
                                 << "/ cause:" << exitInfo.cause();
    SentryService::reportError(
            QStringLiteral("Generic service error"),
            QStringLiteral("request: %1 | %2").arg(toInt(requestNum)).arg(QString::fromStdString(toString(exitInfo))));
    emit genericErrorOccurred();
}

} // namespace KDC
