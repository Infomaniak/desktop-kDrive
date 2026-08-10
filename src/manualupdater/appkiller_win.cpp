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

#include "appkiller.h"

#include "libcommon/utility/utility.h"
#include "libcommonserver/log/log.h"

#include <config.h>

#include <QProcess>
#include <QString>
#include <QStringList>

namespace KDC {

namespace {

QString exeName(const QString &baseName) {
    return baseName + QStringLiteral(".exe");
}

} // namespace

QStringList appProcessNames() {
    return {exeName(QStringLiteral(APPLICATION_EXECUTABLE)), exeName(QStringLiteral(APPLICATION_CLIENT_EXECUTABLE))};
}

// tasklist returns 0 when at least one matching process is found (prints "INFO: ..." to stderr
// when none). We check the stdout content for the image name to be robust.
bool isAnyProcessRunning(const QStringList &names) {
    for (const auto &name: names) {
        QProcess p;
        p.setProgram(QStringLiteral("tasklist"));
        p.setArguments({QStringLiteral("/FI"), QStringLiteral("IMAGENAME eq ") + name, QStringLiteral("/NH")});
        p.start();
        if (!p.waitForFinished(5000)) {
            LOGW_WARN(Log::instance()->getLogger(), L"tasklist timed out checking " << name.toStdWString());
            continue;
        }
        const QString output = QString::fromUtf8(p.readAllStandardOutput());
        if (output.contains(name, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

// taskkill without /F sends WM_CLOSE (soft). With /F it force-terminates (hard).
void signalProcesses(const QStringList &names, const bool force) {
    for (const auto &name: names) {
        QStringList args{QStringLiteral("/IM"), name};
        if (force) {
            args.prepend(QStringLiteral("/F"));
        }
        (void) QProcess::execute(QStringLiteral("taskkill"), args);
    }
}

QString manualKillHint() {
    return QStringLiteral("via Task Manager");
}

} // namespace KDC
