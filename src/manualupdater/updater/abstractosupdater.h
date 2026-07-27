#pragma once

#include "libcommon/utility/types.h"

#include <QString>
#include <functional>
#include <memory>
#include <string>

namespace KDC {

enum class InstallStep {
    Downloading,
    Verifying,
    Preparing,
    Installing,
    Done,
    EnumEnd
};

class AbstractOsUpdater {
    public:
        virtual ~AbstractOsUpdater() = default;

        /**
         * @brief Download and install the specified version.
         * @param versionInfo      Base VersionInfo (URL may be mutated internally).
         * @param progressCallback Called with (step, message) for UI progress updates.
         * @param outMessage       On failure, human-readable error; on success, completion message.
         * @return true if the operation succeeded.
         */
        virtual bool install(const VersionInfo &versionInfo,
                             const std::function<void(InstallStep, const QString &)> &progressCallback, QString &outMessage) = 0;

    protected:
        [[nodiscard]] static bool verifyFileChecksum(const VersionInfo &versionInfo, const SyncPath &filepath,
                                                     QString &outMessage);
        [[nodiscard]] static bool computeFileChecksum(const SyncPath &filepath, std::string &outChecksum);
        [[nodiscard]] static bool createDownloadDirectory(SyncPath &outDir);
};

std::unique_ptr<AbstractOsUpdater> createOsUpdater();

} // namespace KDC
