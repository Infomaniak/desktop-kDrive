#include "sentryhandler.h"
#include "config.h"
#include "version.h"

#include "utility/utility.h"
#include <asserts.h>

namespace KDC {
std::shared_ptr<SentryHandler> SentryHandler::_instance = nullptr;

std::shared_ptr<SentryHandler> SentryHandler::instance() {
    if (!_instance) {
        assert(false && "SentryHandler must be initialized before calling instance");
        return std::shared_ptr<SentryHandler>(new SentryHandler());  // Create a dummy instance to avoid crash but should never
                                                                     // happen (the sentry
                                                                     // will
                                                                     // not be send)
    }
    return _instance;
}

void SentryHandler::init(SentryProject project, int breadCrumbsSize) {
    if (_instance) {
        assert(false && "SentryHandler already initialized");
        return;
    }

    // Sentry init
    sentry_options_t *options = sentry_options_new();
    sentry_options_set_dsn(options, SENTRY_SERVER_DSN);
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    SyncPath appWorkingPath = CommonUtility::getAppWorkingDir() / SENTRY_CRASHPAD_HANDLER_NAME;
#endif

    SyncPath appSupportPath = CommonUtility::getAppSupportDir();
    appSupportPath /= (project == SentryProject::Server) ? SENTRY_SERVER_DB_PATH : SENTRY_CLIENT_DB_PATH;
#if defined(Q_OS_WIN)
    sentry_options_set_handler_pathw(options, appWorkingPath.c_str());
    sentry_options_set_database_pathw(options, appSupportPath.c_str());
#elif defined(Q_OS_MAC)
    sentry_options_set_handler_path(options, appWorkingPath.c_str());
    sentry_options_set_database_path(options, appSupportPath.c_str());
#endif
    sentry_options_set_release(options, KDRIVE_VERSION_STRING);
    sentry_options_set_debug(options, false);
    sentry_options_set_max_breadcrumbs(options, breadCrumbsSize);

    // Set the environment
    bool isSet = false;
    if (std::string environment = CommonUtility::envVarValue("KDRIVE_SENTRY_ENVIRONMENT", isSet); !isSet) {
        // TODO: When the intern/beta update channel will be available, we will have to create a new environment for it.
#ifdef NDEBUG
        sentry_options_set_environment(options, "production");
#else
        sentry_options_set_environment(options, "dev_unknown");
#endif
    } else if (environment.empty()) {  // Disable sentry
        return;
    } else {
        environment = "dev_" + environment;  // We add a prefix to avoid any conflict with the sentry environment.
        sentry_options_set_environment(options, environment.c_str());
    }

    // Init sentry
    ASSERT(sentry_init(options) == 0);
    _instance = std::shared_ptr<SentryHandler>(new SentryHandler());
}

void SentryHandler::setAuthenticatedUser(const SentryUser &user) {
    std::scoped_lock lock(_mutex);
    _authenticatedUser = toSentryValue(user);
    sentry_value_set_by_key(_authenticatedUser, "ip_address", sentry_value_new_string("{{auto}}"));
}

void SentryHandler::captureMessage(SentryLevel level, const std::string &title, const std::string &message, SentryUserType userType,
                                   const SentryUser &user) {
    std::scoped_lock lock(_mutex);
    sentry_value_t userValue;
    switch (userType) {
        case KDC::SentryHandler::SentryUserType::Unknown:
            userValue = toSentryValue(SentryUser());
            sentry_value_set_by_key(userValue, "ip_address", sentry_value_new_string("{{auto}}"));
            sentry_value_set_by_key(userValue, "authentication", sentry_value_new_string("Unknown"));
            break;
        case KDC::SentryHandler::SentryUserType::Authenticated:
            userValue = _authenticatedUser; // TODO: Change this to not copy the value
            sentry_value_set_by_key(userValue, "authentication", sentry_value_new_string("Authenticated"));
            break;
        case KDC::SentryHandler::SentryUserType::Specific:
            userValue = toSentryValue(user);
            sentry_value_set_by_key(userValue, "ip_address", sentry_value_new_string("{{auto}}"));
            sentry_value_set_by_key(userValue, "authentication", sentry_value_new_string("Specific"));

            break;
        default: // KDC::SentryHandler::SentryUserType::Anonymous
            userValue = toSentryValue(SentryUser("Anonymous", "Anonymous", "Anonymous"));
            sentry_value_set_by_key(userValue, "ip_address", sentry_value_new_string("-1.-1.-1.-1"));
            sentry_value_set_by_key(userValue, "authentication", sentry_value_new_string("Anonymous"));
            break;
    }
    sentry_set_user(userValue);
    sentry_capture_event(sentry_value_new_message_event((sentry_level_t)toInt(level), title.c_str(), message.c_str()));
}

sentry_value_t SentryHandler::toSentryValue(const SentryUser &user) {
    sentry_value_t userValue = sentry_value_new_object();
    sentry_value_set_by_key(userValue, "email", sentry_value_new_string(user.getEmail().data()));
    sentry_value_set_by_key(userValue, "username", sentry_value_new_string(user.getUsername().data()));
    sentry_value_set_by_key(userValue, "userId", sentry_value_new_string(user.getUserId().data()));
    return userValue;
}

SentryHandler::~SentryHandler() {
    std::scoped_lock lock(_mutex);
    if (this == _instance.get()) {
        sentry_close();  // Only the instance can close the sentry
    }
}
}  // namespace KDC