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

#include "applicationidentity.h"

#include <config.h>

#include <QGuiApplication>

namespace KDC::ApplicationIdentity {

static constexpr auto applicationIconPath = ":/assets/taskbar/logo_kdrive.svg";

static QIcon applicationIcon() {
    return QIcon(QString::fromLatin1(applicationIconPath));
}

QIcon applyTo() {
    QGuiApplication::setApplicationName(QString::fromLatin1(APPLICATION_CLIENTV4_EXECUTABLE));
    QGuiApplication::setApplicationDisplayName(QString::fromLatin1(APPLICATION_NAME));
    QGuiApplication::setDesktopFileName(QString::fromLatin1(APPLICATION_EXECUTABLE));

    QIcon icon = applicationIcon();
    QGuiApplication::setWindowIcon(icon);
    return icon;
}

} // namespace KDC::ApplicationIdentity
