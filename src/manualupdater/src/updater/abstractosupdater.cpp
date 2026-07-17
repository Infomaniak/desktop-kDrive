#include "abstractosupdater.h"

#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/log/log.h"
#include "libcommon/utility/utility.h"

#include <QObject>

#include <fstream>
#include <array>
#include <Poco/SHA2Engine.h>

#if defined(KD_MACOS)
#include "macosupdater.h"
#elif defined(KD_WINDOWS)
#include "windowsupdater.h"
#else
#include "linuxupdater.h"
#endif

namespace KDUpdater {

std::unique_ptr<AbstractOsUpdater> createOsUpdater() {
#if defined(KD_MACOS)
    return std::make_unique<MacOSUpdater>();
#elif defined(KD_WINDOWS)
    return std::make_unique<WindowsUpdater>();
#else
    return std::make_unique<LinuxUpdater>();
#endif
}

bool AbstractOsUpdater::verifyFileChecksum(const KDC::VersionInfo &versionInfo, const KDC::SyncPath &filepath,
                                           QString &outMessage) {
    const std::string expectedChecksum = KDC::CommonUtility::trim(KDC::CommonUtility::toLower(versionInfo.checksum));
    if (expectedChecksum.empty()) {
        return true;
    }

    const std::string actualChecksum = KDC::CommonUtility::trim(KDC::CommonUtility::toLower(computeFileChecksum(filepath)));
    if (actualChecksum.empty()) {
        LOGW_ERROR(KDC::Log::instance()->getLogger(), L"Failed to compute file checksum.");
        outMessage = QObject::tr("Failed to compute file checksum.");
        auto ioError = KDC::IoError::Success;
        (void) KDC::IoHelper::deleteItem(filepath, ioError);
        return false;
    }

    if (actualChecksum != expectedChecksum) {
        LOGW_ERROR(KDC::Log::instance()->getLogger(), L"Checksum mismatch! Expected: "
                                                              << KDC::CommonUtility::s2ws(expectedChecksum) << L", Got: "
                                                              << KDC::CommonUtility::s2ws(actualChecksum));
        outMessage = QObject::tr("Checksum verification failed.");
        auto ioError = KDC::IoError::Success;
        (void) KDC::IoHelper::deleteItem(filepath, ioError);
        return false;
    }

    LOGW_INFO(KDC::Log::instance()->getLogger(), L"Checksum verification passed.");
    return true;
}

std::string AbstractOsUpdater::computeFileChecksum(const KDC::SyncPath &filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) return "";

    Poco::SHA2Engine sha256(Poco::SHA2Engine::ALGORITHM::SHA_256);
    std::array<char, 8192> buffer{};
    while (file.read(buffer.data(), buffer.size()) || file.gcount() > 0) {
        sha256.update(buffer.data(), static_cast<std::size_t>(file.gcount()));
    }

    return Poco::DigestEngine::digestToHex(sha256.digest());
}

} // namespace KDUpdater
