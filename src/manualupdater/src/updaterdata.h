#pragma once

#include <memory>
#include <string>

#include "libcommon/utility/cstypes.h"

namespace KDC {
class ParmsDb;
}

namespace KDUpdater {

class UpdaterData {
    public:
        bool initialize();

        [[nodiscard]] bool isInstalled() const { return _isInstalled; }
        [[nodiscard]] const std::string &installedVersion() const { return _installedVersion; }
        [[nodiscard]] const std::string &appId() const { return _appId; }
        [[nodiscard]] KDC::DistributionChannel distributionChannel() const { return _distributionChannel; }
        [[nodiscard]] std::shared_ptr<KDC::ParmsDb> db() const { return _db; }

    private:
        static KDC::DistributionChannel defaultDistributionChannel();
        bool _isInstalled = false;
        std::string _installedVersion;
        std::string _appId;
        KDC::DistributionChannel _distributionChannel = defaultDistributionChannel();
        std::shared_ptr<KDC::ParmsDb> _db;
};

} // namespace KDUpdater
