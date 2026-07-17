#include "updaterdata.h"

#include "parmsdblite.h"
#include "libparms/db/parmsdb.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/db/db.h"
#include "libcommonserver/log/log.h"


namespace KDUpdater {

KDC::DistributionChannel UpdaterData::defaultDistributionChannel() {
    LOGW_INFO(KDC::Log::instance()->getLogger(), L"Determining default distribution channel" << KDC::CommonUtility::platform());
    switch (KDC::CommonUtility::platform()) {
        case KDC::Platform::LinuxAMD:
        case KDC::Platform::LinuxARM:
            LOGW_INFO(KDC::Log::instance()->getLogger(), L"Default distribution channel: Prod");
            return KDC::DistributionChannel::Prod;
        case KDC::Platform::MacOS:
        case KDC::Platform::Windows:
        case KDC::Platform::WindowsServer:
        case KDC::Platform::Unknown:
        case KDC::Platform::EnumEnd:
            LOGW_INFO(KDC::Log::instance()->getLogger(), L"Default distribution channel: Internal");
            return KDC::DistributionChannel::Internal;
    }
    return KDC::DistributionChannel::Internal;
}

bool UpdaterData::initialize() {
    bool alreadyExist = false;
    const auto dbPath = KDC::Db::makeDbName(alreadyExist);
    if (dbPath.empty() || !alreadyExist) {
        LOGW_INFO(KDC::Log::instance()->getLogger(), L"kDrive database not found at: " << Path2WStr(dbPath));
        _isInstalled = false;
        return true;
    }

    _db = ParmsDbLite::instance(dbPath);
    if (!_db) {
        LOGW_INFO(KDC::Log::instance()->getLogger(), L"Failed to open kDrive database at: " << Path2WStr(dbPath));
        _isInstalled = false;
        return false;
    }

    LOGW_INFO(KDC::Log::instance()->getLogger(), L"Opened kDrive database at: " << Path2WStr(dbPath));

    bool found = false;
    if (!_db->selectVersion(_installedVersion, found) || !found) {
        LOGW_INFO(KDC::Log::instance()->getLogger(),
                  L"Failed to retrieve kDrive version from database at: " << Path2WStr(dbPath));
        _isInstalled = false;
        return false;
    }

    std::string appUid;
    found = false;
    if (!_db->selectAppUid(appUid, found) || !found) {
        LOGW_WARN(KDC::Log::instance()->getLogger(), L"Failed to retrieve app UID from database");
        _isInstalled = false;
        return false;
    }
    _appId = appUid;

    _isInstalled = true;
    return true;
}

} // namespace KDUpdater
