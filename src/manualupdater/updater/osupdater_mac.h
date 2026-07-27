#pragma once

#include "abstractosupdater.h"

namespace KDC {

class OSUpdater final : public AbstractOsUpdater {
    public:
        bool install(const VersionInfo &versionInfo, const std::function<void(InstallStep, const QString &)> &progressCallback,
                     QString &outMessage) override;

    private:
        static bool downloadAndParseAppcast(const std::string &appcastUrl, const SyncPath &baseDir, QString &outPkgUrl,
                                            QString &outMessage);
};

} // namespace KDC
