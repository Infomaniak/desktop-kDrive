#pragma once

#include "abstractosupdater.h"

namespace KDC {

class WindowsUpdater final : public AbstractOsUpdater {
    public:
        bool install(const VersionInfo &versionInfo, const std::function<void(int32_t, QString)> &progressCallback,
                     QString &outMessage) override;

    private:
        [[nodiscard]] static bool getInstallerPath(const VersionInfo &versionInfo, SyncPath &path);
};

} // namespace KDC
