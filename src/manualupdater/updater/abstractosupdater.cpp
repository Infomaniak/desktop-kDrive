#include "abstractosupdater.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/log/log.h"
#include "libcommon/utility/utility.h"
#include "manualupdater/httpdownloader.h"

#include <QObject>

#include <fstream>
#include <sstream>
#include <array>
#include <algorithm>
#include <cctype>
#include <Poco/SHA2Engine.h>

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

bool AbstractOsUpdater::verifyFileChecksum(const VersionInfo &versionInfo, const std::string &fileUrl, const SyncPath &filepath,
                                           QString &outMessage) {
    // 1. Try fetching the .sha256 sidecar file.
    std::string expectedChecksum;
    // Insert the suffix before any query string or fragment so signed/CDN URLs keep their sidecar resolvable.
    const auto suffixPos = fileUrl.find_first_of("?#");
    const std::string sha256Url =
            fileUrl.substr(0, suffixPos) + ".sha256" +
            (suffixPos == std::string::npos ? std::string{} : fileUrl.substr(suffixPos));

    if (const auto result = HttpDownloader::get(sha256Url, "text/plain, */*"); result.success && !result.body.empty()) {
        // The file contains either "hash" or "hash  filename" — take the first token.
        std::istringstream iss(result.body);
        iss >> expectedChecksum;
    }

    // 2. Fall back to the API-provided checksum.
    if (expectedChecksum.empty()) {
        expectedChecksum = versionInfo.checksum;
    }

    // 3. If still empty, skip verification.
    if (expectedChecksum.empty()) {
        LOGW_INFO(Log::instance()->getLogger(),
                  L"No checksum available for verification, skipping: " << CommonUtility::s2ws(fileUrl));
        return true;
    }

    // 4. Compute the local file's SHA-256.
    std::string actualChecksum;
    if (!computeFileChecksum(filepath, actualChecksum)) {
        outMessage = QObject::tr("Failed to compute file checksum.");
        return false;
    }

    // 5. Compare case-insensitively.
    if (actualChecksum.size() != expectedChecksum.size() ||
        !std::equal(actualChecksum.begin(), actualChecksum.end(), expectedChecksum.begin(), [](char a, char b) {
            return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
        })) {
        outMessage = QObject::tr("Checksum verification failed. The file may be corrupted.");
        LOGW_ERROR(Log::instance()->getLogger(), L"Checksum mismatch for "
                                                         << CommonUtility::s2ws(filepath.string()) << L" — expected: "
                                                         << CommonUtility::s2ws(expectedChecksum) << L" actual: "
                                                         << CommonUtility::s2ws(actualChecksum));
        return false;
    }

    LOGW_INFO(Log::instance()->getLogger(), L"Checksum verified for " << CommonUtility::s2ws(filepath.string()));
    return true;
}

bool AbstractOsUpdater::computeFileChecksum(const SyncPath &filepath, std::string &outChecksum) {
    outChecksum.clear();
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        return false;
    }

    Poco::SHA2Engine sha256(Poco::SHA2Engine::ALGORITHM::SHA_256);
    std::array<char, 8192> buffer{};
    while (file.read(buffer.data(), buffer.size()) || file.gcount() > 0) {
        sha256.update(buffer.data(), static_cast<std::size_t>(file.gcount()));
    }

    if (file.bad()) {
        return false;
    }

    try {
        outChecksum = Poco::DigestEngine::digestToHex(sha256.digest());
    } catch (...) {
        outChecksum.clear(); // Clear again: assignment may have thrown mid-write
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
