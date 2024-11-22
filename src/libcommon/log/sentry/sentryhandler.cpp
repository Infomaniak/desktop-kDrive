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
#include <fstream>
#include <sstream>

#include <asserts.h>

namespace KDC {

std::shared_ptr<SentryHandler> SentryHandler::_instance = nullptr;

KDC::AppType SentryHandler::_appType = KDC::AppType::None;
bool SentryHandler::_debugCrashCallback = false;
bool SentryHandler::_debugBeforeSendCallback = false;

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

    std::cerr << "Sentry detected a crash in the app " << SentryHandler::appType() << std::endl;

    // As `signum` is unknown, a crash is considered as a kill.
    const int signum{0};
    KDC::CommonUtility::writeSignalFile(SentryHandler::appType(), KDC::fromInt<KDC::SignalType>(signum));

    if (SentryHandler::debugCrashCallback()) {
        std::stringstream ss;
        readObject(event, ss);
        SentryHandler::writeEvent(ss.str(), true);
    }

    return event;
}

sentry_value_t beforeSendCallback(sentry_value_t event, void *hint, void *closure) {
    (void) hint;
    (void) closure;

    std::cout << "Sentry will send an event for the app " << SentryHandler::appType() << std::endl;

    if (SentryHandler::debugBeforeSendCallback()) {
        std::stringstream ss;
        readObject(event, ss);
        SentryHandler::writeEvent(ss.str(), false);
    }

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

void SentryHandler::init(KDC::AppType appType, int breadCrumbsSize) {
    if (_instance) {
        assert(false && "SentryHandler already initialized");
        return;
    }

    _instance = std::shared_ptr<SentryHandler>(new SentryHandler());
    if (!_instance) {
        assert(false);
        return;
    }

    if (appType == KDC::AppType::None) {
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
    sentry_options_set_dsn(options, ((appType == KDC::AppType::Server) ? SENTRY_SERVER_DSN : SENTRY_CLIENT_DSN));
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    const SyncPath appWorkingPath = CommonUtility::getAppWorkingDir() / SENTRY_CRASHPAD_HANDLER_NAME;
#endif

    SyncPath appSupportPath = CommonUtility::getAppSupportDir();
    appSupportPath /= (_appType == AppType::Server) ? SENTRY_SERVER_DB_PATH : SENTRY_CLIENT_DB_PATH;
#if defined(Q_OS_WIN)
    sentry_options_set_handler_pathw(options, appWorkingPath.c_str());
    sentry_options_set_database_pathw(options, appSupportPath.c_str());
#elif defined(Q_OS_MAC)
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
    sentry_options_set_traces_sample_rate(options, 0.1); // 10% of traces
#else
    sentry_options_set_traces_sample_rate(options, 1);
#endif
    sentry_options_set_max_spans(options, 1000);

    // Init sentry
    ASSERT(sentry_init(options) == 0);
    _instance->_isSentryActivated = true;
}

void SentryHandler::setAuthenticatedUser(const SentryUser &user) {
    std::scoped_lock lock(_mutex);
    _authenticatedUser = user;
    updateEffectiveSentryUser();
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
    _sentryMaxCaptureCountBeforeRateLimit = static_cast<unsigned int>(std::max(1, maxCaptureCountBeforeRateLimit));
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

bool SentryHandler::lastEventCaptureIsOutdated(const SentryEvent &event) const {
    using namespace std::chrono;
    return (event.lastCapture + seconds(_sentryMinUploadIntervaOnRateLimit)) <= system_clock::now();
}

bool SentryHandler::lastEventUploadIsOutdated(const SentryEvent &event) const {
    using namespace std::chrono;
    return (event.lastUpload + seconds(_sentryMinUploadIntervaOnRateLimit)) <= system_clock::now();
}

void SentryHandler::escalateSentryEvent(SentryEvent &event) const {
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
    if (_globalConfidentialityLevel == SentryConfidentialityLevel::Anonymous) {
        userValue = toSentryValue(SentryUser("Anonymous", "Anonymous", "Anonymous"));
        sentry_value_set_by_key(userValue, "ip_address", sentry_value_new_string("0.0.0.0"));
        sentry_value_set_by_key(userValue, "authentication", sentry_value_new_string("Anonymous"));
    } else if (!user.isDefault()) {
        userValue = toSentryValue(user);
        sentry_value_set_by_key(userValue, "ip_address", sentry_value_new_string("{{auto}}"));
        sentry_value_set_by_key(userValue, "authentication", sentry_value_new_string("Specific"));
    } else if (_globalConfidentialityLevel == SentryConfidentialityLevel::Authenticated) {
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

void SentryHandler::writeEvent(const std::string &eventStr, bool crash) noexcept {
    using namespace KDC::event_dump_files;
    auto eventFilePath =
            std::filesystem::temp_directory_path() / (SentryHandler::appType() == AppType::Server
                                                              ? (crash ? serverCrashEventFileName : serverSendEventFileName)
                                                              : (crash ? clientCrashEventFileName : clientSendEventFileName));

    std::ofstream eventFile(eventFilePath, std::ios::app);
    if (eventFile) {
        eventFile << eventStr << std::endl;
        eventFile.close();
    }
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
                                        SentryConfidentialityLevel confidentialityLevel, const SentryUser &user) :
    title(title),
    message(message), level(level), confidentialityLevel(confidentialityLevel), userId(user.userId()) {}

SentryHandler::PerformanceTrace::PerformanceTrace(pTraceId id) : _pTraceId{id} {
    assert(id != 0 && "operationId must be different from 0");
}

void SentryHandler::stopPTrace(const pTraceId &id, bool aborted) {
    std::scoped_lock lock(_mutex);
    if (id == 0 || !_performanceTraces.contains(id)) return;
    const PerformanceTrace &performanceTrace = _performanceTraces.at(id);

    // Stop any child PerformanceTrace
    std::vector<pTraceId> toDelete;
    for (const auto &childPerformanceTraceId: performanceTrace.children()) {
        toDelete.push_back(childPerformanceTraceId);
    }
    for (auto childPerformanceTraceId2: toDelete) {
        stopPTrace(childPerformanceTraceId2, aborted);
    }

    // Stop the PerformanceTrace
    if (performanceTrace.isSpan()) {
        if (!performanceTrace.span()) {
            assert(false && "Span is not valid");
            return;
        }
        if (aborted) sentry_span_set_status(performanceTrace.span(), SENTRY_SPAN_STATUS_ABORTED);
        sentry_span_finish(performanceTrace.span()); // Automatically stop any child transaction
    } else {
        if (!performanceTrace.transaction()) {
            assert(false && "Transaction is not valid");
            return;
        }
        if (aborted)
            sentry_transaction_set_status(performanceTrace.transaction(),
                                          SENTRY_SPAN_STATUS_ABORTED /*This enum is also valid for a transactrion.*/);
        sentry_transaction_finish(performanceTrace.transaction()); // Automatically stop any child transaction
    }
    _performanceTraces.erase(id);
}

pTraceId SentryHandler::startTransaction(const std::string &name, const std::string &description) {
    sentry_transaction_context_t *tx_ctx = sentry_transaction_context_new(name.c_str(), description.c_str());
    sentry_transaction_t *tx = sentry_transaction_start(tx_ctx, sentry_value_new_null());
    _pTraceIdCounter++;
    pTraceId traceId = _pTraceIdCounter;
    auto [it, res] = _performanceTraces.try_emplace(traceId, traceId);
    if (!res) {
        assert(false && "Transaction already exists");
        return 0;
    }

    PerformanceTrace &pTrace = it->second;
    pTrace.setTransaction(tx);

    return _pTraceIdCounter;
}

pTraceId SentryHandler::startSpan(const std::string &name, const std::string &description,
                                  const pTraceId &parentId) {
    if (!_performanceTraces.contains(parentId)) {
        assert(false && "Parent transaction/span does not exist");
        return 0;
    }

    PerformanceTrace &parent = _performanceTraces.at(parentId);
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

    _pTraceIdCounter++;
    pTraceId traceId = _pTraceIdCounter;
    auto [it, res] = _performanceTraces.try_emplace(traceId, traceId);
    if (!res) {
        assert(false && "Transaction already exists");
        return 0;
    }

    PerformanceTrace &pTrace = it->second;
    pTrace.setSpan(span);
    parent.addChild(pTrace.id());
    return traceId;
}

pTraceId SentryHandler::startPTrace(SentryHandler::PTraceName pTraceName, int syncDbId) {
    std::scoped_lock lock(_mutex);
    if (!_isSentryActivated || pTraceName == SentryHandler::PTraceName::None) return 0;

    if (PTraceInfo pTraceInfo(pTraceName); pTraceInfo._parentPTraceName != SentryHandler::PTraceName::None) {
        // Find the parent
        auto pTraceMap = _pTraceNameToPTraceIdMap.find(syncDbId);
        if (pTraceMap == _pTraceNameToPTraceIdMap.end()) {
            assert(false && "Parent transaction/span is not running.");
            return 0;
        }

        auto parentPTraceIt = pTraceMap->second.find(pTraceInfo._parentPTraceName);
        if (parentPTraceIt == pTraceMap->second.end() || parentPTraceIt->second == 0) {
            assert(false && "Parent transaction/span is not running.");
            return 0;
        }

        const pTraceId &parentId = parentPTraceIt->second;

        // Start the span
        _pTraceNameToPTraceIdMap[syncDbId][pTraceInfo._pTraceName] =
                startSpan(pTraceInfo._pTraceTitle, pTraceInfo._pTraceDescription, parentId);
    } else {
        // Start the transaction
        _pTraceNameToPTraceIdMap[syncDbId][pTraceInfo._pTraceName] =
                startTransaction(pTraceInfo._pTraceTitle, pTraceInfo._pTraceDescription);
    }
    return _pTraceNameToPTraceIdMap[syncDbId][pTraceName];
}

void SentryHandler::stopPTrace(SentryHandler::PTraceName pTraceName, int syncDbId, bool aborted) {
    std::scoped_lock lock(_mutex);
    if (!_isSentryActivated || pTraceName == SentryHandler::PTraceName::None) return;
    if (!_pTraceNameToPTraceIdMap.contains(syncDbId) || !_pTraceNameToPTraceIdMap[syncDbId].contains(pTraceName) ||
        _pTraceNameToPTraceIdMap[syncDbId][pTraceName] == 0) {
        assert(false && "Transaction is not running");
        return;
    }

    stopPTrace(_pTraceNameToPTraceIdMap[syncDbId][pTraceName], aborted);
    _pTraceNameToPTraceIdMap[syncDbId][pTraceName] = 0;
}

SentryHandler::PTraceInfo::PTraceInfo(SentryHandler::PTraceName pTraceName) : _pTraceName{pTraceName} {
    switch (_pTraceName) {
        case SentryHandler::PTraceName::None:
            assert(false && "Transaction must be different from None");
            break;
        case SentryHandler::PTraceName::AppStart:
            _pTraceTitle = "AppStart";
            _pTraceDescription = "Strat the application";
            break;
        case SentryHandler::PTraceName::SyncInit:
            _pTraceTitle = "Synchronisation Init";
            _pTraceDescription = "Synchronisation initialization";
            break;
        case SentryHandler::PTraceName::ResetStatus:
            _pTraceTitle = "ResetStatus";
            _pTraceDescription = "Reseting status";
            _parentPTraceName = SentryHandler::PTraceName::SyncInit;
            break;
        case SentryHandler::PTraceName::Sync:
            _pTraceTitle = "Synchronisation";
            _pTraceDescription = "Synchronisation.";
            break;
        case SentryHandler::PTraceName::UpdateDetection1:
            _pTraceTitle = "UpdateDetection1";
            _pTraceDescription = "Compute FS operations";
            _parentPTraceName = SentryHandler::PTraceName::Sync;
            break;
        case SentryHandler::PTraceName::UpdateUnsyncedList:
            _pTraceTitle = "UpdateUnsyncedList";
            _pTraceDescription = "Update unsynced list";
            _parentPTraceName = SentryHandler::PTraceName::UpdateDetection1;
            break;
        case SentryHandler::PTraceName::InferChangesFromDb:
            _pTraceTitle = "InferChangesFromDb";
            _pTraceDescription = "Infer changes from DB";
            _parentPTraceName = SentryHandler::PTraceName::UpdateDetection1;
            break;
        case SentryHandler::PTraceName::ExploreLocalSnapshot:
            _pTraceTitle = "ExploreLocalSnapshot";
            _pTraceDescription = "Explore local snapshot";
            _parentPTraceName = SentryHandler::PTraceName::UpdateDetection1;
            break;
        case SentryHandler::PTraceName::ExploreRemoteSnapshot:
            _pTraceTitle = "ExploreRemoteSnapshot";
            _pTraceDescription = "Explore remote snapshot";
            _parentPTraceName = SentryHandler::PTraceName::UpdateDetection1;
            break;
        case SentryHandler::PTraceName::UpdateDetection2:
            _pTraceTitle = "UpdateDetection2";
            _pTraceDescription = "UpdateTree generation";
            _parentPTraceName = SentryHandler::PTraceName::Sync;
            break;
        case SentryHandler::PTraceName::ResetNodes:
            _pTraceTitle = "ResetNodes";
            _pTraceDescription = "Reset nodes";
            _parentPTraceName = SentryHandler::PTraceName::UpdateDetection2;
            break;
        case SentryHandler::PTraceName::Step1MoveDirectory:
            _pTraceTitle = "Step1MoveDirectory";
            _pTraceDescription = "Move directory";
            _parentPTraceName = SentryHandler::PTraceName::UpdateDetection2;
            break;
        case SentryHandler::PTraceName::Step2MoveFile:
            _pTraceTitle = "Step2MoveFile";
            _pTraceDescription = "Move file";
            _parentPTraceName = SentryHandler::PTraceName::UpdateDetection2;
            break;
        case SentryHandler::PTraceName::Step3DeleteDirectory:
            _pTraceTitle = "Step3DeleteDirectory";
            _pTraceDescription = "Delete directory";
            _parentPTraceName = SentryHandler::PTraceName::UpdateDetection2;
            break;
        case SentryHandler::PTraceName::Step4DeleteFile:
            _pTraceTitle = "Step4DeleteFile";
            _pTraceDescription = "Delete file";
            _parentPTraceName = SentryHandler::PTraceName::UpdateDetection2;
            break;
        case SentryHandler::PTraceName::Step5CreateDirectory:
            _pTraceTitle = "Step5CreateDirectory";
            _pTraceDescription = "Create directory";
            _parentPTraceName = SentryHandler::PTraceName::UpdateDetection2;
            break;
        case SentryHandler::PTraceName::Step6CreateFile:
            _pTraceTitle = "Step6CreateFile";
            _pTraceDescription = "Create file";
            _parentPTraceName = SentryHandler::PTraceName::UpdateDetection2;
            break;
        case SentryHandler::PTraceName::Step7EditFile:
            _pTraceTitle = "Step7EditFile";
            _pTraceDescription = "Edit file";
            _parentPTraceName = SentryHandler::PTraceName::UpdateDetection2;
            break;
        case SentryHandler::PTraceName::Step8CompleteUpdateTree:
            _pTraceTitle = "Step8CompleteUpdateTree";
            _pTraceDescription = "Complete update tree";
            _parentPTraceName = SentryHandler::PTraceName::UpdateDetection2;
            break;
        case SentryHandler::PTraceName::Reconciliation1:
            _pTraceTitle = "Reconciliation1";
            _pTraceDescription = "Platform inconsistency check";
            _parentPTraceName = SentryHandler::PTraceName::Sync;
            break;
        case SentryHandler::PTraceName::CheckLocalTree:
            _pTraceTitle = "CheckLocalTree";
            _pTraceDescription = "Check local update tree integrity";
            _parentPTraceName = SentryHandler::PTraceName::Reconciliation1;
            break;
        case SentryHandler::PTraceName::CheckRemoteTree:
            _pTraceTitle = "CheckRemoteTree";
            _pTraceDescription = "Check remote update tree integrity";
            _parentPTraceName = SentryHandler::PTraceName::Reconciliation1;
            break;
        case SentryHandler::PTraceName::Reconciliation2:
            _pTraceTitle = "Reconciliation2";
            _pTraceDescription = "Find conflicts";
            _parentPTraceName = SentryHandler::PTraceName::Sync;
            break;
        case SentryHandler::PTraceName::Reconciliation3:
            _pTraceTitle = "Reconciliation3";
            _pTraceDescription = "Resolve conflicts";
            _parentPTraceName = SentryHandler::PTraceName::Sync;
            break;
        case SentryHandler::PTraceName::Reconciliation4:
            _pTraceTitle = "Reconciliation4";
            _pTraceDescription = "Operation Generator";
            _parentPTraceName = SentryHandler::PTraceName::Sync;
            break;
        case SentryHandler::PTraceName::GenerateItemOperations:
            _pTraceTitle = "GenerateItemOperations";
            _pTraceDescription = "Generate the list of operations for 1000 items";
            _parentPTraceName = SentryHandler::PTraceName::Reconciliation4;
            break;
        case SentryHandler::PTraceName::Propagation1:
            _pTraceTitle = "Propagation1";
            _pTraceDescription = "Operation Sorter";
            _parentPTraceName = SentryHandler::PTraceName::Sync;
            break;
        case SentryHandler::PTraceName::Propagation2:
            _pTraceTitle = "Propagation2";
            _pTraceDescription = "Executor";
            _parentPTraceName = SentryHandler::PTraceName::Sync;
            break;
        case SentryHandler::PTraceName::InitProgress:
            _pTraceTitle = "InitProgress";
            _pTraceDescription = "Init the progress manager";
            _parentPTraceName = SentryHandler::PTraceName::Propagation2;
            break;
        case SentryHandler::PTraceName::JobGeneration:
            _pTraceTitle = "JobGeneration";
            _pTraceDescription = "Generate the list of jobs";
            _parentPTraceName = SentryHandler::PTraceName::Propagation2;
            break;
        case SentryHandler::PTraceName::waitForAllJobsToFinish:
            _pTraceTitle = "waitForAllJobsToFinish";
            _pTraceDescription = "Wait for all jobs to finish";
            _parentPTraceName = SentryHandler::PTraceName::Propagation2;
            break;
        case SentryHandler::PTraceName::LFSO_GenerateInitialSnapshot:
            _pTraceTitle = "LFSO_GenerateInitialSnapshot";
            _pTraceDescription = "Explore sync directory";
            break;
        case SentryHandler::PTraceName::LFSO_ExploreItem:
            _pTraceTitle = "LFSO_ExploreItem(x1000)";
            _pTraceDescription = "Discover 1000 local files";
            _parentPTraceName = SentryHandler::PTraceName::LFSO_GenerateInitialSnapshot;
            break;
        case SentryHandler::PTraceName::LFSO_ChangeDetected:
            _pTraceTitle = "LFSO_ChangeDetected";
            _pTraceDescription = "Handle one detected changes";
            break;
        case SentryHandler::PTraceName::RFSO_GenerateInitialSnapshot:
            _pTraceTitle = "RFSO_GenerateInitialSnapshot";
            _pTraceDescription = "Generate snapshot";
            break;
        case SentryHandler::PTraceName::RFSO_BackRequest:
            _pTraceTitle = "RFSO_BackRequest";
            _pTraceDescription = "Request the list of all items to the backend";
            _parentPTraceName = SentryHandler::PTraceName::RFSO_GenerateInitialSnapshot;
            break;
        case SentryHandler::PTraceName::RFSO_ExploreItem:
            _pTraceTitle = "RFSO_ExploreItem(x1000)";
            _pTraceDescription = "Discover 1000 remote files";
            _parentPTraceName = SentryHandler::PTraceName::RFSO_GenerateInitialSnapshot;
            break;
        case SentryHandler::PTraceName::RFSO_ChangeDetected:
            _pTraceTitle = "RFSO_ChangeDetected";
            _pTraceDescription = "Handle one detected changes";
            break;
        default:
            assert(false && "Invalid transaction name");
            break;
    }
}
} // namespace KDC
