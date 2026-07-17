#pragma once

#include "abstractosupdater.h"

namespace KDC {

class LinuxUpdater final : public AbstractOsUpdater {
    public:
        bool install(const VersionInfo &versionInfo, const std::string &desiredVersion,
                     std::function<void(int, QString)> progressCallback, QString &outMessage) override;
};

} // namespace KDC
