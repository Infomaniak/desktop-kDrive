#pragma once

#include "libcommon/utility/types.h"

#include <string>
#include <Poco/Net/HTTPResponse.h>

namespace KDC {

class HttpDownloader {
    public:
        struct Result {
                bool success = false;
                uint16_t statusCode;
                std::string body;
                std::string error;
        };

        static Result get(const std::string &url);
        static Result downloadFile(const std::string &url, const SyncPath &destPath, int64_t timeoutSeconds = 1800);
        static bool fetchAppVersion(DistributionChannel channel, const std::string &appId, VersionInfo &outVersionInfo,
                                    std::string &outError);
};

} // namespace KDC
