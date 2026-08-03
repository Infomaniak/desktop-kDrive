#pragma once

#include "parmsdblite.h"


#include <memory>
#include <string>

#include "libcommon/utility/cstypes.h"


namespace KDC {

class UpdaterData {
    public:
        bool initialize();

        [[nodiscard]] bool isInstalled() const { return _isInstalled; }
        [[nodiscard]] const std::string &installedVersion() const { return _installedVersion; }
        [[nodiscard]] const std::string &appId() const { return _appId; }
        [[nodiscard]] DistributionChannel distributionChannel() const { return _distributionChannel; }
        [[nodiscard]] std::shared_ptr<ParmsDbLite> db() const { return _db; }

    private:
        static DistributionChannel defaultDistributionChannel();
        bool _isInstalled = false;
        std::string _installedVersion;
        std::string _appId;
        DistributionChannel _distributionChannel = defaultDistributionChannel();
        std::shared_ptr<ParmsDbLite> _db;
};

} // namespace KDC
