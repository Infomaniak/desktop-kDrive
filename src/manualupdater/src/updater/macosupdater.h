#pragma once

#include "abstractosupdater.h"

namespace KDUpdater {

class MacOSUpdater final : public AbstractOsUpdater {
    public:
        bool install(const KDC::VersionInfo &versionInfo, const std::string &desiredVersion,
                     std::function<void(int, QString)> progressCallback, QString &outMessage) override;

    private:
        bool downloadAndParseAppcast(const std::string &appcastUrl, QString &outPkgUrl, QString &outMessage);
};

} // namespace KDUpdater
