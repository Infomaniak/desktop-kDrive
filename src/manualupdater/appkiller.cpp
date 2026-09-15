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

#include <chrono>
#include <thread>

namespace KDC {

namespace {

// Returns true if the processes are gone; false if still running or the check failed.
// `queryFailed` is set to true if the process query itself failed (timeout, etc.).
bool waitForProcessesGone(const QStringList &names, const int32_t timeoutMs, bool &queryFailed) {
    queryFailed = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        const auto result = isAnyProcessRunning(names);
        if (result == ProcessCheckResult::QueryFailed) {
            queryFailed = true;
            return false;
        }
        if (result == ProcessCheckResult::NotRunning) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    const auto result = isAnyProcessRunning(names);
    if (result == ProcessCheckResult::QueryFailed) {
        queryFailed = true;
        return false;
    }
    return result == ProcessCheckResult::NotRunning;
}

} // namespace

bool killRunningApp(QString &outMessage) {
    const QStringList names = appProcessNames();

    const auto initialCheck = isAnyProcessRunning(names);
    if (initialCheck == ProcessCheckResult::QueryFailed) {
        outMessage = QStringLiteral(
                             "Could not determine whether kDrive is running. Please make sure kDrive is stopped "
                             "manually (e.g., ") +
                     manualKillHint() + QStringLiteral("), then relaunch this tool.");
        LOGW_ERROR(Log::instance()->getLogger(), L"Failed to query running processes.");
        return false;
    }
    if (initialCheck == ProcessCheckResult::NotRunning) {
        LOG_INFO(Log::instance()->getLogger(), "No running kDrive process detected.");
        return true;
    }

    LOG_INFO(Log::instance()->getLogger(), "Soft-killing kDrive processes...");
    signalProcesses(names, false);
    bool queryFailed = false;
    if (waitForProcessesGone(names, 5000, queryFailed)) {
        LOG_INFO(Log::instance()->getLogger(), "kDrive processes terminated gracefully.");
        return true;
    }
    if (queryFailed) {
        outMessage = QStringLiteral(
                             "Could not determine whether kDrive is still running. Please make sure kDrive is "
                             "stopped manually (e.g., ") +
                     manualKillHint() + QStringLiteral("), then relaunch this tool.");
        LOGW_ERROR(Log::instance()->getLogger(), L"Failed to query running processes after soft kill.");
        return false;
    }

    LOGW_WARN(Log::instance()->getLogger(), L"kDrive did not exit after 5s, hard-killing...");
    signalProcesses(names, true);
    if (waitForProcessesGone(names, 2000, queryFailed)) {
        LOG_INFO(Log::instance()->getLogger(), "kDrive processes killed.");
        return true;
    }
    if (queryFailed) {
        outMessage = QStringLiteral(
                             "Could not determine whether kDrive is still running. Please make sure kDrive is "
                             "stopped manually (e.g., ") +
                     manualKillHint() + QStringLiteral("), then relaunch this tool.");
        LOGW_ERROR(Log::instance()->getLogger(), L"Failed to query running processes after hard kill.");
        return false;
    }

    outMessage = QStringLiteral("kDrive could not be stopped. Please force quit kDrive manually (e.g., ") + manualKillHint() +
                 QStringLiteral("), then relaunch this tool.");
    LOGW_ERROR(Log::instance()->getLogger(), L"Failed to kill kDrive processes.");
    return false;
}

} // namespace KDC
