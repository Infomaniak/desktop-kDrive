#include "parmsdblite.h"

#include "libcommon/utility/types.h"
#include "libcommonserver/log/log.h"

namespace {
constexpr char SELECT_APP_STATE_REQUEST_ID[] = "select_value_from_key";
constexpr char SELECT_APP_STATE_REQUEST[] = "SELECT value FROM app_state WHERE key=?1;";
} // namespace

namespace KDC {

std::shared_ptr<ParmsDbLite> ParmsDbLite::instance(const std::filesystem::path &dbPath) {
    auto db = std::make_shared<ParmsDbLite>(dbPath);
    if (!db->checkConnect()) {
        return nullptr;
    }
    if (!db->init("")) {
        return nullptr;
    }
    return db;
}

bool ParmsDbLite::create(bool &retry) {
    retry = false;
    return true;
}

bool ParmsDbLite::prepare() {
    if (!createAndPrepareRequest(SELECT_APP_STATE_REQUEST_ID, SELECT_APP_STATE_REQUEST)) return false;
    return true;
}

bool ParmsDbLite::upgrade(const std::string &fromVersion, const std::string &toVersion) {
    (void) fromVersion;
    (void) toVersion;
    return true;
}

bool ParmsDbLite::selectAppUid(std::string &appUid, bool &found) {
    const std::scoped_lock lock(_mutex);

    if (!queryResetAndClearBindings(SELECT_APP_STATE_REQUEST_ID)) {
        LOG_WARN(_logger, "Cannot reset query bindings: " << SELECT_APP_STATE_REQUEST_ID);
        return false;
    }
    if (!queryBindValue(SELECT_APP_STATE_REQUEST_ID, 1, static_cast<int>(KDC::AppStateKey::AppUid))) {
        LOG_WARN(_logger, "Cannot bind query value: " << SELECT_APP_STATE_REQUEST_ID);
        return false;
    }

    if (!queryNext(SELECT_APP_STATE_REQUEST_ID, found)) {
        LOG_WARN(_logger, "Error getting query result: " << SELECT_APP_STATE_REQUEST_ID);
        return false;
    }

    if (!found) {
        return true;
    }

    if (!queryStringValue(SELECT_APP_STATE_REQUEST_ID, 0, appUid)) {
        LOG_WARN(_logger, "Error reading string value from query: " << SELECT_APP_STATE_REQUEST_ID);
        return false;
    }

    return true;
}

} // namespace KDC
