#include "abstractosupdater.h"

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

} // namespace KDUpdater
