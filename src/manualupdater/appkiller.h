#pragma once

#include <QString>

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

} // namespace KDC
