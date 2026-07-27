#pragma once

#include "libcommon/utility/types.h"

#include <QString>
#include <functional>
#include <memory>
#include <string>

namespace KDC {

class AbstractOsUpdater {
    public:
        virtual ~AbstractOsUpdater() = default;

        /**
         * @brief Download and install the specified version.
         * @param versionInfo      Base VersionInfo (URL may be mutated internally).
         * @param progressCallback Called with (percent, message) for UI progress updates.
         * @param outMessage       On failure, human-readable error; on success, completion message.
         * @return true if the operation succeeded.
         */
        virtual bool install(const VersionInfo &versionInfo, const std::function<void(int32_t, QString)> &progressCallback,
                             QString &outMessage) = 0;

    protected:
        [[nodiscard]] static bool verifyFileChecksum(const VersionInfo &versionInfo, const SyncPath &filepath,
                                                     QString &outMessage);
        [[nodiscard]] static bool computeFileChecksum(const SyncPath &filepath, std::string &outChecksum);
};

std::unique_ptr<AbstractOsUpdater> createOsUpdater();

} // namespace KDC
