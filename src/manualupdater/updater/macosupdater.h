#pragma once

#include "abstractosupdater.h"

namespace KDC {

class MacOSUpdater final : public AbstractOsUpdater {
    public:
        bool install(const VersionInfo &versionInfo, std::function<void(int32_t, QString)> progressCallback,
                     QString &outMessage) override;

    private:
        static bool downloadAndParseAppcast(const std::string &appcastUrl, QString &outPkgUrl, QString &outMessage);
};

} // namespace KDC
