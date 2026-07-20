#pragma once

#include "abstractosupdater.h"

namespace KDC {

class LinuxUpdater final : public AbstractOsUpdater {
    public:
        bool install(const VersionInfo &versionInfo, const std::function<void(int32_t, QString)> &progressCallback,
                     QString &outMessage) override;
};

} // namespace KDC
