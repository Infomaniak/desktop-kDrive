#include "sentryhandler.h"
#include "config.h"
#include "version.h"

#include "utility/utility.h"
#include <asserts.h>

namespace KDC {

constexpr const int SENTRY_MAX_CAPTURE_COUNT_BEFORE_RATE_LIMIT = 1;
constexpr const int SENTRY_MINUTES_BETWEEN_UPLOAD_ON_RATE_LIMIT = 10;
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
    _authenticatedUser = user;
}

void SentryHandler::setGlobalConfidentialityLevel(SentryConfidentialityLevel level) {
    assert(level != SentryConfidentialityLevel::Specific && "Specific user type is not allowed as global confidentiality level");
    std::scoped_lock lock(_mutex);
    _globalConfidentialityLevel = level;
}

void SentryHandler::captureMessage(SentryLevel level, const std::string &title, /*Copy needed*/ std::string message,
                                   SentryConfidentialityLevel confidentialityLevel, const SentryUser &user) {
    std::scoped_lock lock(_mutex);
    SentryEvent event(title, message, level, confidentialityLevel, user);
    if (auto it = _events.find(event.getHash()); it != _events.end()) {
        auto storedEvent = it->second;
        storedEvent.captureCount++;
        if (storedEvent.lastCapture + std::chrono::minutes(SENTRY_MINUTES_BETWEEN_UPLOAD_ON_RATE_LIMIT) <
            std::chrono::system_clock::now()) {  // Reset the capture count if the last capture was more than 10 minutes ago
            storedEvent.captureCount = 0;
        } else if (storedEvent.captureCount >= SENTRY_MAX_CAPTURE_COUNT_BEFORE_RATE_LIMIT) {
            event.lastCapture = std::chrono::system_clock::now();
            if (storedEvent.lastUpload + std::chrono::minutes(SENTRY_MINUTES_BETWEEN_UPLOAD_ON_RATE_LIMIT) >
                    std::chrono::system_clock::now() &&
                storedEvent.captureCount != SENTRY_MAX_CAPTURE_COUNT_BEFORE_RATE_LIMIT) {  // Rate limit reached for this event wait 10
                                                                                   // minutes before sending it again
                return;
            } else {
                storedEvent.lastUpload = std::chrono::system_clock::now();
                message += " (Rate limit reached: " + std::to_string(storedEvent.captureCount) +
                           " captures since last app start. Level escalated from " + toString(level) + " to Error)";
                level = SentryLevel::Error;
            }
        } else {
            event.lastCapture = std::chrono::system_clock::now();
        }
    } else {
        event.lastCapture = std::chrono::system_clock::now();
        event.lastUpload = std::chrono::system_clock::now();
        _events.try_emplace(event.getHash(), event);
        //assert(_events.size() < 100 && "SentryHandler::captureMessage: Too many events in the list");
    }

    if (confidentialityLevel != _lastConfidentialityLevel || confidentialityLevel == SentryConfidentialityLevel::Specific) {
        _lastConfidentialityLevel = confidentialityLevel;
        sentry_value_t userValue;
        switch (confidentialityLevel) {
            case KDC::SentryHandler::SentryConfidentialityLevel::Unknown:
                userValue = toSentryValue(SentryUser());
                sentry_value_set_by_key(userValue, "ip_address", sentry_value_new_string("{{auto}}"));
                sentry_value_set_by_key(userValue, "authentication", sentry_value_new_string("Unknown"));
                break;
            case KDC::SentryHandler::SentryConfidentialityLevel::Authenticated:
                userValue = toSentryValue(_authenticatedUser);
                sentry_value_set_by_key(userValue, "authentication", sentry_value_new_string("Authenticated"));
                break;
            case KDC::SentryHandler::SentryConfidentialityLevel::Specific:
                userValue = toSentryValue(user);
                sentry_value_set_by_key(userValue, "ip_address", sentry_value_new_string("{{auto}}"));
                sentry_value_set_by_key(userValue, "authentication", sentry_value_new_string("Specific"));

                break;
            default:  // KDC::SentryHandler::SentryUserType::Anonymous
                userValue = toSentryValue(SentryUser("Anonymous", "Anonymous", "Anonymous"));
                sentry_value_set_by_key(userValue, "ip_address", sentry_value_new_string("-1.-1.-1.-1"));
                sentry_value_set_by_key(userValue, "authentication", sentry_value_new_string("Anonymous"));
                break;
        }

        sentry_remove_user();
        sentry_set_user(userValue);
    }
   // sentry_capture_event(sentry_value_new_message_event((sentry_level_t)toInt(level), title.c_str(), message.c_str()));
}

sentry_value_t SentryHandler::toSentryValue(const SentryUser &user) {
    sentry_value_t userValue = sentry_value_new_object();
    sentry_value_set_by_key(userValue, "email", sentry_value_new_string(user.getEmail().data()));
    sentry_value_set_by_key(userValue, "name", sentry_value_new_string(user.getUsername().data()));
    sentry_value_set_by_key(userValue, "id", sentry_value_new_string(user.getUserId().data()));
    return userValue;
}

SentryHandler::~SentryHandler() {
    if (this == _instance.get()) {
        _instance.reset();
    }
}

SentryHandler::SentryEvent::SentryEvent(const std::string &title, const std::string &message, SentryLevel level,
                                        SentryConfidentialityLevel confidentialityLevel, const SentryUser &user)
    : title(title), message(message), level(level), confidentialityLevel(confidentialityLevel), userId(user.getUserId()) {}
}  // namespace KDC