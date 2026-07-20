#pragma once

#include "abstractosupdater.h"

namespace KDC {

class WindowsUpdater final : public AbstractOsUpdater {
    public:
        bool install(const VersionInfo &versionInfo, std::function<void(int32_t, QString)> progressCallback,
                     QString &outMessage) override;

    private:
        [[nodiscard]] static bool getInstallerPath(const VersionInfo &versionInfo, SyncPath &path);
        [[nodiscard]] static bool verifyDigitalSignature(const SyncPath &filepath, QString &outMessage);
};

} // namespace KDC
