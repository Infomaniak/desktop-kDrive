#pragma once

#include "libcommon/utility/types.h"

#include <string>

namespace KDC {

class HttpDownloader {
    public:
        struct Result {
                bool success = false;
                int statusCode = 0;
                std::string body;
                std::string error;
        };

        static Result get(const std::string &url);
        static Result downloadFile(const std::string &url, const SyncPath &destPath, long timeoutSeconds = 1800);
        static bool fetchAppVersion(DistributionChannel channel, const std::string &appId, VersionInfo &outVersionInfo,
                                    std::string &outError);
};

} // namespace KDC
