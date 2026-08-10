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

#include "libcommonserver/log/log.h"

#include <chrono>
#include <thread>

namespace KDC {

namespace {

bool waitForProcessesGone(const QStringList &names, int timeoutMs) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (!isAnyProcessRunning(names)) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    return !isAnyProcessRunning(names);
}

} // namespace

bool killRunningApp(QString &outMessage) {
    const QStringList names = appProcessNames();

    if (!isAnyProcessRunning(names)) {
        LOG_INFO(Log::instance()->getLogger(), "No running kDrive process detected.");
        return true;
    }

    LOG_INFO(Log::instance()->getLogger(), "Soft-killing kDrive processes...");
    signalProcesses(names, false);
    if (waitForProcessesGone(names, 5000)) {
        LOG_INFO(Log::instance()->getLogger(), "kDrive processes terminated gracefully.");
        return true;
    }

    LOGW_WARN(Log::instance()->getLogger(), L"kDrive did not exit after 5s, hard-killing...");
    signalProcesses(names, true);
    if (waitForProcessesGone(names, 2000)) {
        LOG_INFO(Log::instance()->getLogger(), "kDrive processes killed.");
        return true;
    }

    outMessage = QStringLiteral("kDrive could not be stopped. Please force quit kDrive manually (e.g., ") + manualKillHint() +
                 QStringLiteral("), then relaunch this tool.");
    LOGW_ERROR(Log::instance()->getLogger(), L"Failed to kill kDrive processes.");
    return false;
}

} // namespace KDC
