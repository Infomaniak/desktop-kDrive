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

#include "appclientlinux.h"
#include "libcommon/utility/utility.h"

#include <QLockFile>
#include <QString>

int main(int argc, char *argv[]) {
    if (QLockFile lockFile(QString::fromStdString((KDC::CommonUtility::getAppSupportDir() / "kdrive-client.lock").string()));
        !lockFile.tryLock()) {
        qWarning("kDrive client is already running.");
        return EXIT_FAILURE;
    }

    // see https://doc.qt.io/qt-6/qapplication.html#details
    const QScopedPointer app(new KDC::AppClientLinux(argc, argv));
    const int32_t exitCode = app->exec();

    qCInfo(KDC::lcAppClientLinux) << "Qt event loop exited with code" << exitCode;
    return exitCode;
}
