#pragma once

#include "abstractosupdater.h"

namespace KDUpdater {

class LinuxUpdater final : public AbstractOsUpdater {
    public:
        bool install(const KDC::VersionInfo &versionInfo, const std::string &desiredVersion,
                     std::function<void(int, QString)> progressCallback, QString &outMessage) override;
};

} // namespace KDUpdater
