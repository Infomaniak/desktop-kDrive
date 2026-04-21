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

#include "serviceutils.h"
#include "libcommon/utility/types.h"

namespace KDC::ServiceUtils {

/**
 * Not final version
 * TODO Subject to changements / will depends on the ui implemented
 */
QString formatExitInfo(const ExitInfo &exitInfo) {
    return QStringLiteral("IPC request failed (code=%1[%2], cause=%3[%4])")
            .arg(QString::fromStdString(toString(exitInfo.code())))
            .arg(toInt(exitInfo.code()))
            .arg(QString::fromStdString(toString(exitInfo.cause())))
            .arg(toInt(exitInfo.cause()));
}

} // namespace KDC::ServiceUtils
