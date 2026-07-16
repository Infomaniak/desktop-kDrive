#pragma once

#include "libcommon/utility/types.h"

#include <string>

namespace KDUpdater {

class HttpDownloader {
    public:
        struct Result {
                bool success = false;
                int statusCode = 0;
                std::string body;
                std::string error;
        };

        static Result get(const std::string &url);
        static bool fetchAppVersion(KDC::DistributionChannel channel, const std::string &appId, KDC::VersionInfo &outVersionInfo,
                                    std::string &outError);
};

} // namespace KDUpdater
