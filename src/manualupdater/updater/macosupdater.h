#pragma once

#include "abstractosupdater.h"

namespace KDC {

class MacOSUpdater final : public AbstractOsUpdater {
    public:
        bool install(const VersionInfo &versionInfo, const std::string &desiredVersion,
                     std::function<void(int, QString)> progressCallback, QString &outMessage) override;

    private:
        static bool downloadAndParseAppcast(const std::string &appcastUrl, QString &outPkgUrl, QString &outMessage);
        [[nodiscard]] static bool verifyPackageSignature(const SyncPath &pkgPath, QString &outMessage);
};

} // namespace KDC
