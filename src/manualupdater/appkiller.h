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

#pragma once
#include <QStringList>

namespace KDC {
/**
 * @brief Stop the running kDrive app (server + client) before opening the parameters database.
 *
 * Escalates from a soft kill (graceful termination) to a hard kill (SIGKILL / /F) if the
 * process is still alive after a 5-second grace period.
 *
 * @param outMessage On failure, a human-readable message describing what went wrong.
 * @return true if both processes are dead (or were never running); false if a process
 *         could not be killed.
 */
bool killRunningApp(QString &outMessage);
// ---- Platform hooks (implemented in appkiller_<os>.cpp) ----

/**
 * @brief Executable names to look for and signal. Windows appends ".exe".
 */
QStringList appProcessNames();

/**
 * @brief True if at least one of the named processes is currently running.
 */
bool isAnyProcessRunning(const QStringList &names);

/**
 * @brief Send a termination signal (soft) or force-kill (hard) to the named processes.
 */
void signalProcesses(const QStringList &names, bool force);

/**
 * @brief OS-specific hint appended to the failure message (e.g. "via Activity Monitor").
 */
QString manualKillHint();
} // namespace KDC
