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
#include <chrono>
#include <thread>

namespace KDC {

namespace {

QString exeName(const QString &baseName) {
    return baseName + QStringLiteral(".exe");
}

// tasklist returns 0 when at least one matching process is found (prints "INFO: ..." to stderr
// when none). We check the stdout content for the image name to be robust.
bool isAnyProcessRunning(const QStringList &imageNames) {
    for (const auto &name : imageNames) {
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
void signalProcesses(const QStringList &imageNames, bool force) {
    for (const auto &name : imageNames) {
        QStringList args{QStringLiteral("/IM"), name};
        if (force) {
            args.prepend(QStringLiteral("/F"));
        }
        (void) QProcess::execute(QStringLiteral("taskkill"), args);
    }
}

bool waitForProcessesGone(const QStringList &imageNames, int timeoutMs) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (!isAnyProcessRunning(imageNames)) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    return !isAnyProcessRunning(imageNames);
}

} // namespace

bool killRunningApp(QString &outMessage) {
    const QStringList imageNames{
            exeName(QStringLiteral(APPLICATION_EXECUTABLE)),
            exeName(QStringLiteral(APPLICATION_CLIENT_EXECUTABLE)),
    };

    if (!isAnyProcessRunning(imageNames)) {
        LOG_INFO(Log::instance()->getLogger(), "No running kDrive process detected.");
        return true;
    }

    LOG_INFO(Log::instance()->getLogger(), "Soft-killing kDrive processes...");
    signalProcesses(imageNames, false);
    if (waitForProcessesGone(imageNames, 5000)) {
        LOG_INFO(Log::instance()->getLogger(), "kDrive processes terminated gracefully.");
        return true;
    }

    LOGW_WARN(Log::instance()->getLogger(), L"kDrive did not exit after 5s, hard-killing...");
    signalProcesses(imageNames, true);
    if (waitForProcessesGone(imageNames, 2000)) {
        LOG_INFO(Log::instance()->getLogger(), "kDrive processes killed.");
        return true;
    }

    outMessage = QStringLiteral("kDrive could not be stopped. Please force quit kDrive manually (e.g., via Task Manager), "
                               "then relaunch this tool.");
    LOGW_ERROR(Log::instance()->getLogger(), L"Failed to kill kDrive processes.");
    return false;
}

} // namespace KDC
