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

// pgrep returns 0 when at least one matching process is found.
// Use a non-static QProcess with output suppressed so PIDs don't leak to the terminal.
bool isAnyProcessRunning(const QStringList &processNames) {
    for (const auto &name : processNames) {
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

// killall sends SIGTERM by default (soft) or SIGKILL with -9 (hard).
void signalProcesses(const QStringList &processNames, const QStringList &extraArgs) {
    for (const auto &name : processNames) {
        QStringList args = extraArgs;
        args.append(name);
        (void) QProcess::execute(QStringLiteral("killall"), args);
    }
}

bool waitForProcessesGone(const QStringList &processNames, int timeoutMs) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (!isAnyProcessRunning(processNames)) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    return !isAnyProcessRunning(processNames);
}

} // namespace

bool killRunningApp(QString &outMessage) {
    const QStringList processNames{
            QStringLiteral(APPLICATION_EXECUTABLE),
            QStringLiteral(APPLICATION_CLIENT_EXECUTABLE),
    };

    if (!isAnyProcessRunning(processNames)) {
        LOG_INFO(Log::instance()->getLogger(), "No running kDrive process detected.");
        return true;
    }

    LOG_INFO(Log::instance()->getLogger(), "Soft-killing kDrive processes...");
    signalProcesses(processNames, {});
    if (waitForProcessesGone(processNames, 5000)) {
        LOG_INFO(Log::instance()->getLogger(), "kDrive processes terminated gracefully.");
        return true;
    }

    LOGW_WARN(Log::instance()->getLogger(), L"kDrive did not exit after 5s, hard-killing...");
    signalProcesses(processNames, {QStringLiteral("-9")});
    if (waitForProcessesGone(processNames, 2000)) {
        LOG_INFO(Log::instance()->getLogger(), "kDrive processes killed.");
        return true;
    }

    outMessage = QStringLiteral("kDrive could not be stopped. Please force quit kDrive manually (e.g., via Activity Monitor), "
                               "then relaunch this tool.");
    LOGW_ERROR(Log::instance()->getLogger(), L"Failed to kill kDrive processes.");
    return false;
}

} // namespace KDC
