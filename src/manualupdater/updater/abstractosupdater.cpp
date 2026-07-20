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

namespace KDC {

std::unique_ptr<AbstractOsUpdater> createOsUpdater() {
#if defined(KD_MACOS)
    return std::make_unique<MacOSUpdater>();
#elif defined(KD_WINDOWS)
    return std::make_unique<WindowsUpdater>();
#else
    return std::make_unique<LinuxUpdater>();
#endif
}

bool AbstractOsUpdater::verifyFileChecksum(const VersionInfo &versionInfo, const SyncPath &filepath, QString &outMessage) {
    (void) versionInfo;
    (void) filepath;
    (void) outMessage;
    return true; // placeholder for a future PR
}

std::string AbstractOsUpdater::computeFileChecksum(const SyncPath &filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) return "";

    Poco::SHA2Engine sha256(Poco::SHA2Engine::ALGORITHM::SHA_256);
    std::array<char, 8192> buffer{};
    while (file.read(buffer.data(), buffer.size()) || file.gcount() > 0) {
        sha256.update(buffer.data(), static_cast<std::size_t>(file.gcount()));
    }

    return Poco::DigestEngine::digestToHex(sha256.digest());
}

} // namespace KDC
