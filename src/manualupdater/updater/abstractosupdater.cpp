#include "abstractosupdater.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/utility/checksumverifier.h"
#include "libcommon/utility/utility.h"
#include "manualupdater/httpdownloader.h"

#include <QObject>
#include <filesystem>
#include <sstream>

#if defined(KD_MACOS)
#include "osupdater_mac.h"
#elif defined(KD_WINDOWS)
#include "osupdater_win.h"
#else
#include "osupdater_linux.h"
#endif

namespace KDC {

std::unique_ptr<AbstractOsUpdater> createOsUpdater() {
    return std::make_unique<OSUpdater>();
}

bool AbstractOsUpdater::verifyChecksum(const SyncPath &filepath, const std::string &downloadUrl, QString &outMessage) {
    const auto fetcher = [](const std::string &url) -> std::string {
        const auto result = HttpDownloader::get(url, "text/plain, */*");
        if (!result.success || result.body.empty()) return {};
        std::istringstream iss(result.body);
        std::string checksum;
        iss >> checksum;
        return checksum;
    };
    if (const std::string error; !ChecksumVerifier::verifyFileChecksum(filepath, downloadUrl, fetcher)) {
        outMessage = QObject::tr("Checksum verification failed. The file may be corrupted.");
        return false;
    }
    return true;
}

bool AbstractOsUpdater::createDownloadDirectory(SyncPath &outDir) {
    SyncPath tmpDir;
    if (const auto exitInfo = CommonUtility::deviceTempDirectoryPath(tmpDir); !exitInfo) {
        return false;
    }

    try {
        const auto subDir = tmpDir / ("kDrive_" + CommonUtility::generateRandomStringAlphaNum(10));
        (void) std::filesystem::create_directories(subDir);
        outDir = subDir;
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace KDC
