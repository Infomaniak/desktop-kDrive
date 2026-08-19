#include "updaterdata.h"

#include "parmsdblite.h"
#include "libparms/db/parmsdb.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/db/db.h"
#include "libcommonserver/log/log.h"


namespace KDC {

DistributionChannel UpdaterData::defaultDistributionChannel() {
    switch (CommonUtility::platform()) {
        case Platform::LinuxAMD:
        case Platform::LinuxARM:
            return DistributionChannel::Prod;
        case Platform::MacOS:
        case Platform::Windows:
        case Platform::WindowsServer:
            return DistributionChannel::Internal;
        case Platform::EnumEnd:
            return DistributionChannel::EnumEnd;
        case Platform::Unknown:
        default:
            return DistributionChannel::Unknown;
    }
}

bool UpdaterData::initialize() {
    bool alreadyExist = false;
    const auto dbPath = Db::makeDbName(alreadyExist);
    if (dbPath.empty() || !alreadyExist) {
        LOGW_WARN(Log::instance()->getLogger(), L"kDrive database not found at: " << Path2WStr(dbPath));
        _isInstalled = false;
        return false;
    }

    _db = ParmsDbLite::instance(dbPath);
    if (!_db) {
        LOGW_WARN(Log::instance()->getLogger(), L"Failed to open kDrive database at: " << Path2WStr(dbPath));
        _isInstalled = false;
        return false;
    }

    LOGW_INFO(Log::instance()->getLogger(), L"Opened kDrive database at: " << Path2WStr(dbPath));

    bool found = false;
    if (!_db->selectVersion(_installedVersion, found) || !found) {
        LOGW_WARN(Log::instance()->getLogger(), L"Failed to retrieve kDrive version from database at: " << Path2WStr(dbPath));
        _isInstalled = false;
        return false;
    }

    found = false;
    if (!_db->selectAppUid(_appId, found) || !found) {
        LOGW_WARN(Log::instance()->getLogger(), L"Failed to retrieve app UID from database");
        _isInstalled = false;
        return false;
    }

    _isInstalled = true;
    return true;
}
} // namespace KDC
