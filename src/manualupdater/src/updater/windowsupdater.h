#pragma once

#include "abstractosupdater.h"

namespace KDUpdater {

class WindowsUpdater final : public AbstractOsUpdater {
    public:
        bool install(const KDC::VersionInfo &versionInfo, const std::string &desiredVersion,
                     std::function<void(int, QString)> progressCallback, QString &outMessage) override;

    private:
        [[nodiscard]] static bool getInstallerPath(const KDC::VersionInfo &versionInfo, KDC::SyncPath &path);
        [[nodiscard]] static bool verifyDigitalSignature(const KDC::SyncPath &filepath, QString &outMessage);
};

} // namespace KDUpdater
