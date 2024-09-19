/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "sentryhandler.h"
#include "config.h"
#include "version.h"
#include "utility/utility.h"

#include <iostream>
#include <asserts.h>

namespace KDC {

std::shared_ptr<SentryHandler> SentryHandler::_instance = nullptr;

/*
 *  sentry_value_t reader implementation - begin
 *  Used for debbuging
 */

#define TAG_MASK 0x3

typedef struct {
    union {
        void *_ptr;
        double _double;
    } payload;
    long refcount;
    uint8_t type;
} thing_t;

typedef struct {
    char *k;
    sentry_value_t v;
} obj_pair_t;

typedef struct {
    obj_pair_t *pairs;
    size_t len;
    size_t allocated;
} obj_t;

typedef struct {
    sentry_value_t *items;
    size_t len;
    size_t allocated;
} list_t;

static thing_t *valueAsThing(sentry_value_t value)
{
    if (value._bits & TAG_MASK) {
        return nullptr;
    }
    return (thing_t *)(size_t)value._bits;
}

static void readObject(const sentry_value_t event, int level = 0);
static void readList(const sentry_value_t event, int level = 0);

static void printValue(const sentry_value_t value, int level) {
    switch (sentry_value_get_type(value)) {
        case SENTRY_VALUE_TYPE_NULL:
            std::cout << "NULL" << std::endl;
            break;
        case SENTRY_VALUE_TYPE_BOOL:
            std::cout << (sentry_value_is_true(value) ? "true" : "false") << std::endl;
            break;
        case SENTRY_VALUE_TYPE_INT32:
            std::cout << sentry_value_as_int32(value) << std::endl;
            break;
        case SENTRY_VALUE_TYPE_DOUBLE:
            std::cout << sentry_value_as_double(value) << std::endl;
            break;
        case SENTRY_VALUE_TYPE_STRING:
            std::cout << sentry_value_as_string(value) << std::endl;
            break;
        case SENTRY_VALUE_TYPE_LIST: {
            std::cout << "LIST" << std::endl;
            readList(value, level + 1);
            break;
        }
        case SENTRY_VALUE_TYPE_OBJECT: {
            std::cout << "OBJECT" << std::endl;
            readObject(value, level + 1);
            break;
        }
    }
}

static void readList(const sentry_value_t event, int level) {
    if (sentry_value_get_type(event) != SENTRY_VALUE_TYPE_LIST) {
        return;
    }

    const list_t *l = (list_t *)valueAsThing(event)->payload._ptr;
    const std::string indent(level, ' ');
    for (size_t i = 0; i < l->len; i++) {
        std::cout << indent.c_str() << "val=";
        printValue(l->items[i], level);
    }
}

static void readObject(const sentry_value_t event, int level) {
    if (sentry_value_get_type(event) != SENTRY_VALUE_TYPE_OBJECT) {
        return;
    }

    const thing_t *thing = valueAsThing(event);
    if (!thing) {
        return;
    }

    const obj_t *obj = (obj_t *)thing->payload._ptr;
    const std::string indent(level, ' ');
    for (size_t i = 0; i < obj->len; i++) {
        char *key = obj->pairs[i].k;
        std::cout << indent.c_str() << "key=" << key << " val=";
        printValue(obj->pairs[i].v, level);
    }
}

/*
 *  sentry_value_t reader implementation - end
 */

static sentry_value_t crashCallback(const sentry_ucontext_t *uctx, sentry_value_t event, void *closure) {
    (void)uctx;
    (void)closure;

    readObject(event);

    // signum is unknown, all crashes will be considered as kills
    KDC::SignalType signalType = KDC::fromInt<KDC::SignalType>(0);
    std::cerr << "Server stopped with signal " << signalType << std::endl;

    KDC::CommonUtility::writeSignalFile(KDC::AppType::Server, signalType);

    return event;
}

sentry_value_t beforeSendCallback(sentry_value_t event, void *hint, void *closure) {
    (void)hint;
    (void)closure;

    readObject(event);

    return event;
}

std::shared_ptr<SentryHandler> SentryHandler::instance() {
    if (!_instance) {
        assert(false && "SentryHandler must be initialized before calling instance");
        // TODO: When the logger will be moved to the common library, add a log there.
        return std::shared_ptr<SentryHandler>(new SentryHandler()); // Create a dummy instance to avoid crash but should never
                                                                    // happen (the sentry will not be sent)
    }
    return _instance;
}

void SentryHandler::init(SentryProject project, int breadCrumbsSize) {
    if (_instance) {
        assert(false && "SentryHandler already initialized");
        return;
    }

    _instance = std::shared_ptr<SentryHandler>(new SentryHandler());

    if (project == SentryProject::Deactivated) {
        _instance->_isSentryActivated = false;
        return;
    }

    // Sentry init
    sentry_options_t *options = sentry_options_new();
    sentry_options_set_dsn(options, ((project == SentryProject::Server) ? SENTRY_SERVER_DSN : SENTRY_CLIENT_DSN));
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    const SyncPath appWorkingPath = CommonUtility::getAppWorkingDir() / SENTRY_CRASHPAD_HANDLER_NAME;
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

    bool isSet = false;
#if defined(Q_OS_LINUX)
    if (CommonUtility::envVarValue("KDRIVE_DEBUG_SENTRY_CRASH_CB", isSet); isSet) {
        sentry_options_set_on_crash(options, crashCallback, NULL);
    }
    if (CommonUtility::envVarValue("KDRIVE_DEBUG_SENTRY_BEFORE_SEND_CB", isSet); isSet) {
        sentry_options_set_before_send(options, beforeSendCallback, nullptr);
    }
#endif

    // Set the environment
    isSet = false;
    if (std::string environment = CommonUtility::envVarValue("KDRIVE_SENTRY_ENVIRONMENT", isSet); !isSet) {
        // TODO: When the intern/beta update channel will be available, we will have to create a new environment for each.
#ifdef NDEBUG
        sentry_options_set_environment(options, "production");
#else
        sentry_options_set_environment(options, "dev_unknown");
#endif
    } else if (environment.empty()) { // Disable sentry
        _instance->_isSentryActivated = false;
        return;
    } else {
        environment = "dev_" + environment; // We add a prefix to avoid any conflict with the sentry environment.
        sentry_options_set_environment(options, environment.c_str());
    }

    // Init sentry
    ASSERT(sentry_init(options) == 0);
    _instance->_isSentryActivated = true;
}

void SentryHandler::setAuthenticatedUser(const SentryUser &user) {
    std::scoped_lock lock(_mutex);
    _authenticatedUser = user;
}

void SentryHandler::setGlobalConfidentialityLevel(SentryConfidentialityLevel level) {
    std::scoped_lock lock(_mutex);
    _globalConfidentialityLevel = level;
}

void SentryHandler::captureMessage(SentryLevel level, const std::string &title, std::string message /*Copy needed*/,
                                   const SentryUser &user /*Apply only if confidentiallity level is Authenticated*/) {
    std::scoped_lock lock(_mutex);
    if (!_isSentryActivated) return;

    SentryEvent event(title, message, level, _globalConfidentialityLevel, user);
    bool toUpload = false;
    handleEventsRateLimit(event, toUpload);
    if (!toUpload) return;

    updateEffectiveSentryUser(user);

    sendEventToSentry(event.level, event.title, event.message);
}

void SentryHandler::sendEventToSentry(const SentryLevel level, const std::string &title, const std::string &message) const {
    sentry_capture_event(sentry_value_new_message_event(static_cast<sentry_level_t>(level), title.c_str(), message.c_str()));
}

void SentryHandler::setMaxCaptureCountBeforeRateLimit(int maxCaptureCountBeforeRateLimit) {
    assert(maxCaptureCountBeforeRateLimit > 0 && "Max capture count before rate limit must be greater than 0");
    _sentryMaxCaptureCountBeforeRateLimit = std::max(1, maxCaptureCountBeforeRateLimit);
}

void SentryHandler::setMinUploadIntervalOnRateLimit(int minUploadIntervalOnRateLimit) {
    assert(minUploadIntervalOnRateLimit > 0 && "Min upload interval on rate limit must be greater than 0");
    _sentryMinUploadIntervaOnRateLimit = std::max(1, minUploadIntervalOnRateLimit);
}

sentry_value_t SentryHandler::toSentryValue(const SentryUser &user) const {
    sentry_value_t userValue = sentry_value_new_object();
    sentry_value_set_by_key(userValue, "email", sentry_value_new_string(user.email().data()));
    sentry_value_set_by_key(userValue, "name", sentry_value_new_string(user.username().data()));
    sentry_value_set_by_key(userValue, "id", sentry_value_new_string(user.userId().data()));
    return userValue;
}

void SentryHandler::handleEventsRateLimit(SentryEvent &event, bool &toUpload) {
    using namespace std::chrono;
    std::scoped_lock lock(_mutex);

    toUpload = true;

    auto it = _events.find(event.getStr());
    if (it == _events.end()) {
        event.lastCapture = system_clock::now();
        event.lastUpload = system_clock::now();
        _events.try_emplace(event.getStr(), event);
        return;
    }

    auto &storedEvent = it->second;
    storedEvent.captureCount = std::min(storedEvent.captureCount + 1, UINT_MAX-1);
    event.captureCount = storedEvent.captureCount;

    if (lastEventCaptureIsOutdated(storedEvent)) { // Reset the capture count if the last capture was more than 10 minutes ago
        storedEvent.captureCount = 1;
        storedEvent.lastCapture = system_clock::now();
        storedEvent.lastUpload = system_clock::now();
        return;
    }

    storedEvent.lastCapture = system_clock::now();
    if (storedEvent.captureCount < _sentryMaxCaptureCountBeforeRateLimit) { // Rate limit not reached, we can send the event
        storedEvent.lastUpload = system_clock::now();
        return;
    }

    if (storedEvent.captureCount == _sentryMaxCaptureCountBeforeRateLimit) { // Rate limit reached, we send this event and we will
                                                                             // wait 10 minutes before sending it again
        storedEvent.lastUpload = system_clock::now();
        escalateSentryEvent(event);
        return;
    }

    if (!lastEventUploadIsOutdated(storedEvent)) { // Rate limit reached for this event: wait 10 minutes before sending it again
        toUpload = false;
        return;
    }
    storedEvent.lastUpload = system_clock::now();
    escalateSentryEvent(event);
}

bool SentryHandler::lastEventCaptureIsOutdated(const SentryEvent &event) const {
    using namespace std::chrono;
    return (event.lastCapture + seconds(_sentryMinUploadIntervaOnRateLimit)) <= system_clock::now();
}

bool SentryHandler::lastEventUploadIsOutdated(const SentryEvent &event) const {
    using namespace std::chrono;
    return (event.lastUpload + seconds(_sentryMinUploadIntervaOnRateLimit)) <= system_clock::now();
}

void SentryHandler::escalateSentryEvent(SentryEvent &event) {
    event.message += " (Rate limit reached: " + std::to_string(event.captureCount) + " captures.";
    if (event.level == SentryLevel::Fatal || event.level == SentryLevel::Error) {
        event.message += ")";
        return;
    }

    event.message += " Level escalated from " + toString(event.level) + " to Error)";
    event.level = SentryLevel::Error;
}

void SentryHandler::updateEffectiveSentryUser(const SentryUser &user) {
    if (_globalConfidentialityLevel == _lastConfidentialityLevel && user.isDefault()) return;
    _lastConfidentialityLevel = user.isDefault() ? _globalConfidentialityLevel : SentryConfidentialityLevel::Specific;

    sentry_value_t userValue;
    if (!user.isDefault()) {
        userValue = toSentryValue(user);
        sentry_value_set_by_key(userValue, "ip_address", sentry_value_new_string("{{auto}}"));
        sentry_value_set_by_key(userValue, "authentication", sentry_value_new_string("Specific"));
    } else if (_globalConfidentialityLevel == SentryConfidentialityLevel::Authenticated) {
        userValue = toSentryValue(_authenticatedUser);
        sentry_value_set_by_key(userValue, "ip_address", sentry_value_new_string("{{auto}}"));
        sentry_value_set_by_key(userValue, "authentication", sentry_value_new_string("Authenticated"));
    } else { // Anonymous
        userValue = toSentryValue(SentryUser("Anonymous", "Anonymous", "Anonymous"));
        sentry_value_set_by_key(userValue, "ip_address", sentry_value_new_string("-1.-1.-1.-1"));
        sentry_value_set_by_key(userValue, "authentication", sentry_value_new_string("Anonymous"));
    }

    sentry_remove_user();
    sentry_set_user(userValue);
}

SentryHandler::~SentryHandler() {
    if (this == _instance.get()) {
        _instance.reset();
#if !defined(Q_OS_LINUX)
        try {
            sentry_close();
        } catch (...) {
            // Do nothing
        }
#endif
    }
}

SentryHandler::SentryEvent::SentryEvent(const std::string &title, const std::string &message, SentryLevel level,
                                        SentryConfidentialityLevel confidentialityLevel, const SentryUser &user) 
    : title(title), message(message), level(level), confidentialityLevel(confidentialityLevel), userId(user.userId()) {}
} // namespace KDC
