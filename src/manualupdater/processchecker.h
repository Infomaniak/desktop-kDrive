#pragma once

#include <QString>

namespace KDUpdater {

/**
 * @brief Detects running kDrive processes and optionally terminates them.
 *
 * The public API is entirely synchronous (it sleeps internally) so it can
 * be called from the main thread right before launching the background
 * install thread.
 */
class ProcessChecker {
    public:
        /**
         * @return true if any kDrive process (server or GUI) is currently running.
         */
        static bool isKDriveRunning();

        /**
         * @brief Attempt a graceful quit, wait, then force-kill if needed.
         *
         * @param outMessage   On failure, human-readable reason.
         * @return true  if no kDrive process is running after the attempt.
         *         false if processes could not be terminated (outMessage is set).
         */
        static bool terminateKDrive(QString &outMessage);
};

} // namespace KDUpdater
