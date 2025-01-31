/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
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

#include "handler.h"
#include "config.h"
#include "version.h"
#include "utility/utility.h"

#include <iostream>
#include <fstream>
#include <sstream>

#include <asserts.h>

namespace KDC::sentry {

std::shared_ptr<Handler> Handler::_instance = nullptr;

AppType Handler::_appType = AppType::None;
bool Handler::_debugCrashCallback = false;
bool Handler::_debugBeforeSendCallback = false;

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

static thing_t *valueAsThing(sentry_value_t value) {
    if (value._bits & TAG_MASK) {
        return nullptr;
    }
    return (thing_t *) (size_t) value._bits;
}

static void readObject(const sentry_value_t event, std::stringstream &ss, int level = 0);
static void readList(const sentry_value_t event, std::stringstream &ss, int level = 0);

static void printValue(std::stringstream &ss, const sentry_value_t value, int level) {
    switch (sentry_value_get_type(value)) {
        case SENTRY_VALUE_TYPE_NULL:
            ss << "NULL" << std::endl;
            break;
        case SENTRY_VALUE_TYPE_BOOL:
            ss << (sentry_value_is_true(value) ? "true" : "false") << std::endl;
            break;
        case SENTRY_VALUE_TYPE_INT32:
            ss << sentry_value_as_int32(value) << std::endl;
            break;
        case SENTRY_VALUE_TYPE_DOUBLE:
            ss << sentry_value_as_double(value) << std::endl;
            break;
        case SENTRY_VALUE_TYPE_STRING:
            ss << sentry_value_as_string(value) << std::endl;
            break;
        case SENTRY_VALUE_TYPE_LIST: {
            ss << "LIST" << std::endl;
            readList(value, ss, level + 1);
            break;
        }
        case SENTRY_VALUE_TYPE_OBJECT: {
            ss << "OBJECT" << std::endl;
            readObject(value, ss, level + 1);
            break;
        }
    }
}

static void readList(const sentry_value_t event, std::stringstream &ss, int level) {
    if (sentry_value_get_type(event) != SENTRY_VALUE_TYPE_LIST) {
        return;
    }

    const list_t *l = (list_t *) valueAsThing(event)->payload._ptr;
    const std::string indent(static_cast<size_t>(level), ' ');
    for (size_t i = 0; i < l->len; i++) {
        ss << indent.c_str() << "val=";
        printValue(ss, l->items[i], level);
    }
}

static void readObject(const sentry_value_t event, std::stringstream &ss, int level) {
    if (sentry_value_get_type(event) != SENTRY_VALUE_TYPE_OBJECT) {
        return;
    }

    const thing_t *thing = valueAsThing(event);
    if (!thing) {
        return;
    }

    const obj_t *obj = (obj_t *) thing->payload._ptr;
    const std::string indent(static_cast<size_t>(level), ' ');
    for (size_t i = 0; i < obj->len; i++) {
        char *key = obj->pairs[i].k;
        ss << indent.c_str() << "key=" << key << " val=";
        printValue(ss, obj->pairs[i].v, level);
    }
}

/*
 *  sentry_value_t reader implementation - end
 */

static sentry_value_t crashCallback(const sentry_ucontext_t *uctx, sentry_value_t event, void *closure) {
    (void) uctx;
    (void) closure;

    std::cerr << "Sentry detected a crash in the app " << Handler::appType() << std::endl;

    // As `signum` is unknown, a crash is considered as a kill.
    const int signum{0};
    KDC::CommonUtility::writeSignalFile(Handler::appType(), KDC::fromInt<KDC::SignalType>(signum));

    if (Handler::debugCrashCallback()) {
        std::stringstream ss;
        readObject(event, ss);
        Handler::writeEvent(ss.str(), true);
    }

    return event;
}

sentry_value_t beforeSendCallback(sentry_value_t event, void *hint, void *closure) {
    (void) hint;
    (void) closure;

    std::cout << "Sentry will send an event for the app " << Handler::appType() << std::endl;

    if (Handler::debugBeforeSendCallback()) {
        std::stringstream ss;
        readObject(event, ss);
        Handler::writeEvent(ss.str(), false);
    }

    return event;
}

std::shared_ptr<Handler> Handler::instance() {
    if (!_instance) {
        assert(false && "Handler must be initialized before calling instance");
        // TODO: When the logger will be moved to the common library, add a log there.
        return std::shared_ptr<Handler>(new Handler()); // Create a dummy instance to avoid crash but should never
                                                        // happen (the sentry will not be sent)
    }
    return _instance;
}

void Handler::init(AppType appType, int breadCrumbsSize) {
    if (_instance) {
        assert(false && "Handler already initialized");
        return;
    }

    _instance = std::shared_ptr<Handler>(new Handler());
    if (!_instance) {
        assert(false);
        return;
    }

    if (appType == AppType::None) {
        _instance->_isSentryActivated = false;
        return;
    }

    _appType = appType;

    // For debugging: if the following environment variable is set, the crash event will be printed into a debug file
    bool isSet = false;
    if (CommonUtility::envVarValue("KDRIVE_DEBUG_SENTRY_CRASH_CB", isSet); isSet) {
        _debugCrashCallback = true;
    }

    // For debbuging: if the following environment variable is set, the send events will be printed into a debug file
    // If this variable is set, the previous one is inoperative
    isSet = false;
    if (CommonUtility::envVarValue("KDRIVE_DEBUG_SENTRY_BEFORE_SEND_CB", isSet); isSet) {
        _debugBeforeSendCallback = true;
    }

    // Sentry init
    sentry_options_t *options = sentry_options_new();
    sentry_options_set_dsn(options, ((appType == AppType::Server) ? SENTRY_SERVER_DSN : SENTRY_CLIENT_DSN));
#if defined(Q_OS_WIN) || defined(Q_OS_MAC) || defined(__x86_64__)
    // TODO: On Linux arm64, Sentry is built with breakpad instead of crashpad_handler until support of Ubuntu 20.04 is
    // discontinued
    const SyncPath appWorkingPath = CommonUtility::getAppWorkingDir() / SENTRY_CRASHPAD_HANDLER_NAME;
#endif

    SyncPath appSupportPath = CommonUtility::getAppSupportDir();
    appSupportPath /= (_appType == AppType::Server) ? SENTRY_SERVER_DB_PATH : SENTRY_CLIENT_DB_PATH;
#if defined(Q_OS_WIN)
    sentry_options_set_handler_pathw(options, appWorkingPath.c_str());
    sentry_options_set_database_pathw(options, appSupportPath.c_str());
#elif defined(Q_OS_MAC) || defined(__x86_64__)
    sentry_options_set_handler_path(options, appWorkingPath.c_str());
    sentry_options_set_database_path(options, appSupportPath.c_str());
#endif
    sentry_options_set_release(options, KDRIVE_VERSION_STRING);
    sentry_options_set_debug(options, false);
    sentry_options_set_max_breadcrumbs(options, static_cast<size_t>(breadCrumbsSize));

    // !!! Not Supported in Crashpad on macOS & Limitations in Crashpad on Windows for Fast-fail Crashes !!!
    // See https://docs.sentry.io/platforms/native/configuration/filtering/
    if (_debugBeforeSendCallback) {
        sentry_options_set_before_send(options, beforeSendCallback, nullptr);
    } else {
        sentry_options_set_on_crash(options, crashCallback, nullptr);
    }

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

#ifdef NDEBUG
    sentry_options_set_traces_sample_rate(options, 0.001); // 0.1% of traces will be sent to sentry.
#else
    sentry_options_set_traces_sample_rate(options, 1);
#endif
    sentry_options_set_max_spans(options, 1000); // Maximum number of spans per transaction

    // Init sentry
    int res = sentry_init(options);
    std::cerr << "sentry_init returned " << res << std::endl;
    ASSERT(res == 0);
    _instance->_isSentryActivated = true;
}

void Handler::init(const std::shared_ptr<Handler> &initializedHandler) {
    if (_instance) {
        return;
    }
    _instance = initializedHandler;
}

void Handler::setAuthenticatedUser(const SentryUser &user) {
    std::scoped_lock lock(_mutex);
    _authenticatedUser = user;
    updateEffectiveSentryUser();
}

void Handler::setGlobalConfidentialityLevel(sentry::ConfidentialityLevel level) {
    std::scoped_lock lock(_mutex);
    _globalConfidentialityLevel = level;
}

void Handler::privateCaptureMessage(Level level, const std::string &title, std::string message /*Copy needed*/,
                                    const SentryUser &user /*Apply only if confidentiallity level is Authenticated*/) {
    if (!_isSentryActivated) return;

    std::scoped_lock lock(_mutex);

    SentryEvent event(title, message, level, _globalConfidentialityLevel, user);
    bool toUpload = false;
    handleEventsRateLimit(event, toUpload);
    if (!toUpload) return;

    updateEffectiveSentryUser(user);

    sendEventToSentry(event.level, event.title, event.message);
}

void Handler::sendEventToSentry(const Level level, const std::string &title, const std::string &message) const {
    sentry_capture_event(sentry_value_new_message_event(static_cast<sentry_level_t>(level), title.c_str(), message.c_str()));
}

void Handler::setMaxCaptureCountBeforeRateLimit(int maxCaptureCountBeforeRateLimit) {
    assert(maxCaptureCountBeforeRateLimit > 0 && "Max capture count before rate limit must be greater than 0");
    _sentryMaxCaptureCountBeforeRateLimit = static_cast<unsigned int>(std::max(1, maxCaptureCountBeforeRateLimit));
}

void Handler::setMinUploadIntervalOnRateLimit(int minUploadIntervalOnRateLimit) {
    assert(minUploadIntervalOnRateLimit > 0 && "Min upload interval on rate limit must be greater than 0");
    _sentryMinUploadIntervalOnRateLimit = std::max(1, minUploadIntervalOnRateLimit);
}

sentry_value_t Handler::toSentryValue(const SentryUser &user) const {
    sentry_value_t userValue = sentry_value_new_object();
    sentry_value_set_by_key(userValue, "email", sentry_value_new_string(user.email().data()));
    sentry_value_set_by_key(userValue, "name", sentry_value_new_string(user.username().data()));
    sentry_value_set_by_key(userValue, "id", sentry_value_new_string(user.userId().data()));
    return userValue;
}

void Handler::handleEventsRateLimit(SentryEvent &event, bool &toUpload) {
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
    storedEvent.captureCount = std::min(storedEvent.captureCount + 1, UINT_MAX - 1);
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

    if (storedEvent.captureCount == _sentryMaxCaptureCountBeforeRateLimit) { // Rate limit reached, we send this event and we
                                                                             // will wait 10 minutes before sending it again
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

bool Handler::lastEventCaptureIsOutdated(const SentryEvent &event) const {
    using namespace std::chrono;
    return (event.lastCapture + seconds(_sentryMinUploadIntervalOnRateLimit)) <= system_clock::now();
}

bool Handler::lastEventUploadIsOutdated(const SentryEvent &event) const {
    using namespace std::chrono;
    return (event.lastUpload + seconds(_sentryMinUploadIntervalOnRateLimit)) <= system_clock::now();
}

void Handler::escalateSentryEvent(SentryEvent &event) const {
    event.message += " (Rate limit reached: " + std::to_string(event.captureCount) + " captures.";
    if (event.level == Level::Fatal || event.level == Level::Error) {
        event.message += ")";
        return;
    }

    event.message += " Level escalated from " + toString(event.level) + " to Error)";
    event.level = Level::Error;
}

void Handler::updateEffectiveSentryUser(const SentryUser &user) {
    if (_globalConfidentialityLevel == _lastConfidentialityLevel && user.isDefault()) return;
    _lastConfidentialityLevel = user.isDefault() ? _globalConfidentialityLevel : sentry::ConfidentialityLevel::Specific;

    sentry_value_t userValue;
    if (_globalConfidentialityLevel == sentry::ConfidentialityLevel::Anonymous) {
        userValue = toSentryValue(SentryUser("Anonymous", "Anonymous", "Anonymous"));
        sentry_value_set_by_key(userValue, "ip_address", sentry_value_new_string("0.0.0.0"));
        sentry_value_set_by_key(userValue, "authentication", sentry_value_new_string("Anonymous"));
    } else if (!user.isDefault()) {
        userValue = toSentryValue(user);
        sentry_value_set_by_key(userValue, "ip_address", sentry_value_new_string("{{auto}}"));
        sentry_value_set_by_key(userValue, "authentication", sentry_value_new_string("Specific"));
    } else if (_globalConfidentialityLevel == sentry::ConfidentialityLevel::Authenticated) {
        userValue = toSentryValue(_authenticatedUser);
        sentry_value_set_by_key(userValue, "ip_address", sentry_value_new_string("{{auto}}"));
        sentry_value_set_by_key(userValue, "authentication", sentry_value_new_string("Authenticated"));
    } else {
        assert(false && "Invalid _globalConfidentialityLevel");
        sentry_remove_user();
        return;
    }

    sentry_remove_user();
    sentry_set_user(userValue);
}

void Handler::writeEvent(const std::string &eventStr, bool crash) noexcept {
    using namespace KDC::event_dump_files;
    auto eventFilePath = std::filesystem::temp_directory_path() /
                         (Handler::appType() == AppType::Server ? (crash ? serverCrashEventFileName : serverSendEventFileName)
                                                                : (crash ? clientCrashEventFileName : clientSendEventFileName));

    std::ofstream eventFile(eventFilePath, std::ios::app);
    if (eventFile) {
        eventFile << eventStr << std::endl;
        eventFile.close();
    }
}

Handler::~Handler() {
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

Handler::SentryEvent::SentryEvent(const std::string &title, const std::string &message, Level level,
                                  sentry::ConfidentialityLevel confidentialityLevel, const SentryUser &user) :
    title(title), message(message), level(level), confidentialityLevel(confidentialityLevel), userId(user.userId()) {}

void Handler::stopPTrace(const pTraceId &id, PTraceStatus status) {
    if (id == 0) return;

    std::scoped_lock lock(_mutex);
    auto performanceTraceIt = _pTraces.find(id);
    if (performanceTraceIt == _pTraces.end()) {
        return;
    }
    PerformanceTrace &performanceTrace = performanceTraceIt->second;

    // Stop any child PerformanceTrace
    for (const auto &childPerformanceTraceId: performanceTrace.children()) {
        stopPTrace(childPerformanceTraceId, status);
    }

    // Stop the PerformanceTrace
    if (performanceTrace.isSpan()) {
        if (!performanceTrace.span()) {
            assert(false && "Span is not valid");
            return;
        }
        sentry_span_set_status(performanceTrace.span(), static_cast<sentry_span_status_t>(status));
        sentry_span_finish(performanceTrace.span()); // Automatically stop any child transaction
    } else {
        if (!performanceTrace.transaction()) {
            assert(false && "Transaction is not valid");
            return;
        }
        sentry_transaction_set_status(performanceTrace.transaction(), static_cast<sentry_span_status_t>(status));
        sentry_transaction_finish(performanceTrace.transaction()); // Automatically stop any child transaction
    }
    _pTraces.erase(id);
}

pTraceId Handler::makeUniquePTraceId() {
    bool reseted = false;
    do {
        _pTraceIdCounter++;
        if (_pTraceIdCounter >= std::numeric_limits<pTraceId>::max() - 1) {
            if (reseted) {
                assert(false && "No more unique pTraceId available");
                return 0;
            }
            _pTraceIdCounter = 1;
            reseted = true;
        }
    } while (_pTraces.contains(_pTraceIdCounter)); // Ensure the pTraceId is unique
    return _pTraceIdCounter;
}

pTraceId Handler::startTransaction(const std::string &name, const std::string &description) {
    sentry_transaction_context_t *tx_ctx = sentry_transaction_context_new(name.c_str(), description.c_str());
    sentry_transaction_t *tx = sentry_transaction_start(tx_ctx, sentry_value_new_null());
    pTraceId traceId = makeUniquePTraceId();
    auto [it, res] = _pTraces.try_emplace(traceId, traceId);
    if (!res) {
        assert(false && "Transaction already exists");
        return 0;
    }

    PerformanceTrace &pTrace = it->second;
    pTrace.setTransaction(tx);

    return _pTraceIdCounter;
}

pTraceId Handler::startSpan(const std::string &name, const std::string &description, const pTraceId &parentId) {
    auto parentIt = _pTraces.find(parentId);
    if (parentIt == _pTraces.end()) {
        assert(false && "Parent transaction/span does not exist");
        return 0;
    }

    PerformanceTrace &parent = parentIt->second;
    sentry_span_t *span = nullptr;
    if (parent.isSpan()) {
        if (!parent.span()) {
            assert(false && "Parent span is not valid");
            return 0;
        }
        span = sentry_span_start_child(parent.span(), name.c_str(), description.c_str());
        assert(span);
    } else {
        if (!parent.transaction()) {
            assert(false && "Parent transaction is not valid");
            return 0;
        }
        span = sentry_transaction_start_child(parent.transaction(), name.c_str(), description.c_str());
        assert(span);
    }

    pTraceId traceId = makeUniquePTraceId();
    auto [it, res] = _pTraces.try_emplace(traceId, traceId);
    if (!res) {
        assert(false && "Transaction already exists");
        return 0;
    }

    PerformanceTrace &pTrace = it->second;
    pTrace.setSpan(span);
    parent.addChild(pTrace.id());
    return traceId;
}

pTraceId Handler::startPTrace(const PTraceDescriptor &pTraceInfo, int syncDbId) {
    if (!_isSentryActivated || pTraceInfo._pTraceName == PTraceName::None) return 0;

    std::scoped_lock lock(_mutex);
    pTraceId newPTraceId = 0;
    if (pTraceInfo._parentPTraceName != PTraceName::None) {
        // Fetch the pTraceMap associated with the provided syncDbId
        auto pTraceMapIt = _pTraceNameToPTraceIdMap.find(syncDbId);
        if (pTraceMapIt == _pTraceNameToPTraceIdMap.end()) {
            assert(false && "Parent transaction/span is not running.");
            return 0;
        }
        auto &pTraceMap = pTraceMapIt->second;

        // Find the parent pTrace
        auto parentPTraceIt = pTraceMap.find(pTraceInfo._parentPTraceName);
        if (parentPTraceIt == pTraceMap.end() || parentPTraceIt->second == 0) {
            assert(false && "Parent transaction/span is not running.");
            return 0;
        }

        const pTraceId &parentId = parentPTraceIt->second;

        // Start the span
        newPTraceId = startSpan(pTraceInfo._pTraceTitle, pTraceInfo._pTraceDescription, parentId);
        _pTraceNameToPTraceIdMap[syncDbId][pTraceInfo._pTraceName] = newPTraceId;
    } else {
        // Start the transaction
        newPTraceId = startTransaction(pTraceInfo._pTraceTitle, pTraceInfo._pTraceDescription);
        _pTraceNameToPTraceIdMap[syncDbId][pTraceInfo._pTraceName] = newPTraceId;
    }
    return newPTraceId;
}

void Handler::stopPTrace(const PTraceDescriptor &pTraceInfo, int syncDbId, PTraceStatus status) {
    if (!_isSentryActivated || pTraceInfo._pTraceName == PTraceName::None) return;

    std::scoped_lock lock(_mutex);
    auto pTraceMapIt = _pTraceNameToPTraceIdMap.find(syncDbId);
    if (pTraceMapIt == _pTraceNameToPTraceIdMap.end()) {
        assert(false && "Transaction/span is not running.");
        return;
    }
    auto &pTraceMap = pTraceMapIt->second;

    auto pTraceIt = pTraceMap.find(pTraceInfo._pTraceName);
    if (pTraceIt == pTraceMap.end() || pTraceIt->second == 0) {
        assert(false && "Transaction is not running");
        return;
    }

    stopPTrace(pTraceIt->second, status);
    pTraceIt->second = 0;
}

} // namespace KDC::sentry
