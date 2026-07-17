#include "updaterdata.h"

#include "parmsdblite.h"
#include "libparms/db/parmsdb.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/db/db.h"
#include "libcommonserver/log/log.h"


namespace KDC {

DistributionChannel UpdaterData::defaultDistributionChannel() {
    LOGW_INFO(Log::instance()->getLogger(), L"Determining default distribution channel" << CommonUtility::platform());
    switch (CommonUtility::platform()) {
        case Platform::LinuxAMD:
        case Platform::LinuxARM:
            LOGW_INFO(Log::instance()->getLogger(), L"Default distribution channel: Prod");
            return DistributionChannel::Prod;
        case Platform::MacOS:
        case Platform::Windows:
        case Platform::WindowsServer:
        case Platform::Unknown:
        case Platform::EnumEnd:
            LOGW_INFO(Log::instance()->getLogger(), L"Default distribution channel: Internal");
            return DistributionChannel::Internal;
    }
    return DistributionChannel::Internal;
}

bool UpdaterData::initialize() {
    bool alreadyExist = false;
    const auto dbPath = Db::makeDbName(alreadyExist);
    if (dbPath.empty() || !alreadyExist) {
        LOGW_INFO(Log::instance()->getLogger(), L"kDrive database not found at: " << Path2WStr(dbPath));
        _isInstalled = false;
        return true;
    }

    _db = ParmsDbLite::instance(dbPath);
    if (!_db) {
        LOGW_INFO(Log::instance()->getLogger(), L"Failed to open kDrive database at: " << Path2WStr(dbPath));
        _isInstalled = false;
        return false;
    }

    LOGW_INFO(Log::instance()->getLogger(), L"Opened kDrive database at: " << Path2WStr(dbPath));

    bool found = false;
    if (!_db->selectVersion(_installedVersion, found) || !found) {
        LOGW_INFO(Log::instance()->getLogger(),
                  L"Failed to retrieve kDrive version from database at: " << Path2WStr(dbPath));
        _isInstalled = false;
        return false;
    }

    std::string appUid;
    found = false;
    if (!_db->selectAppUid(appUid, found) || !found) {
        LOGW_WARN(Log::instance()->getLogger(), L"Failed to retrieve app UID from database");
        _isInstalled = false;
        return false;
    }
    _appId = appUid;

    _isInstalled = true;
    return true;
}

} // namespace KDC
