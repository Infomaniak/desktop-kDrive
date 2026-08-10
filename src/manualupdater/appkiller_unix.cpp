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

QStringList appProcessNames() {
    return {QStringLiteral(APPLICATION_EXECUTABLE), QStringLiteral(APPLICATION_CLIENT_EXECUTABLE)};
}

// pgrep returns 0 when at least one matching process is found.
// Use a non-static QProcess with output suppressed so PIDs don't leak to the terminal.
bool isAnyProcessRunning(const QStringList &names) {
    for (const auto &name: names) {
        QProcess p;
        p.setProgram(QStringLiteral("pgrep"));
        p.setArguments({QStringLiteral("-x"), name});
        p.setStandardOutputFile(QProcess::nullDevice());
        p.setStandardErrorFile(QProcess::nullDevice());
        p.start();
        if (!p.waitForFinished(5000)) {
            LOGW_WARN(Log::instance()->getLogger(), L"pgrep timed out checking " << name.toStdWString());
            continue;
        }
        if (p.exitCode() == 0) {
            return true;
        }
    }
    return false;
}

// Linux: pkill with -x for exact matching (so the updater itself is never killed).
// macOS: killall matches by exact name by default.
// Both send SIGTERM by default (soft) or SIGKILL with -9 (hard).
void signalProcesses(const QStringList &names, const bool force) {
    for (const auto &name: names) {
        QStringList args;
#if defined(Q_OS_LINUX)
        args.append(QStringLiteral("-x"));
#endif
        if (force) {
            args.append(QStringLiteral("-9"));
        }
        args.append(name);
#if defined(Q_OS_LINUX)
        (void) QProcess::execute(QStringLiteral("pkill"), args);
#else
        (void) QProcess::execute(QStringLiteral("killall"), args);
#endif
    }
}

QString manualKillHint() {
#if defined(Q_OS_LINUX)
    return QStringLiteral("via System Monitor or `kill -9`");
#else
    return QStringLiteral("via Activity Monitor");
#endif
}

} // namespace KDC
