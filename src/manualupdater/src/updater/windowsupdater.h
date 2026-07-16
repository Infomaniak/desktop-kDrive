#pragma once

#include "abstractosupdater.h"

namespace KDUpdater {

class WindowsUpdater final : public AbstractOsUpdater {
    public:
        bool install(const KDC::VersionInfo &versionInfo, const std::string &desiredVersion,
                     std::function<void(int, QString)> progressCallback, QString &outMessage) override;

    private:
        [[nodiscard]] bool getInstallerPath(const KDC::VersionInfo &versionInfo, KDC::SyncPath &path) const;
        [[nodiscard]] bool verifyFileChecksum(const KDC::VersionInfo &versionInfo, const KDC::SyncPath &filepath,
                                                QString &outMessage) const;
        [[nodiscard]] std::string computeFileChecksum(const KDC::SyncPath &filepath) const;
        [[nodiscard]] bool verifyDigitalSignature(const KDC::SyncPath &filepath, QString &outMessage) const;
};

} // namespace KDUpdater
