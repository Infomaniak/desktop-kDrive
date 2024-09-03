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
#include <asserts.h>

namespace KDC {
constexpr const int SentryMaxCaptureCountBeforeRateLimit = 10;  // Number of captures of the same event before rate limiting
constexpr const int SentryMinUploadIntervaOnRateLimit =
    10;  // Number of minutes to wait before sending the event again after rate limiting
std::shared_ptr<SentryHandler> SentryHandler::_instance = nullptr;

std::shared_ptr<SentryHandler> SentryHandler::instance() {
    if (!_instance) {
        assert(false && "SentryHandler must be initialized before calling instance");
        // TODO: When the logger will be moved to the common library, add a log there.
        return std::shared_ptr<SentryHandler>(new SentryHandler());  // Create a dummy instance to avoid crash but should never
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

    if (project == SentryProject::Desactivated) {
        _instance->_isSentryActivated = false;
        return;
    }

    // Sentry init
    sentry_options_t *options = sentry_options_new();
    sentry_options_set_dsn(options, SENTRY_SERVER_DSN);
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

    // Set the environment
    bool isSet = false;
    if (std::string environment = CommonUtility::envVarValue("KDRIVE_SENTRY_ENVIRONMENT", isSet); !isSet) {
        // TODO: When the intern/beta update channel will be available, we will have to create a new environment for each.
#ifdef NDEBUG
        sentry_options_set_environment(options, "production");
#else
        sentry_options_set_environment(options, "dev_unknown");
#endif
    } else if (environment.empty()) {  // Disable sentry
        _instance->_isSentryActivated = false;
        return;
    } else {
        environment = "dev_" + environment;  // We add a prefix to avoid any conflict with the sentry environment.
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

    sentry_capture_event(
        sentry_value_new_message_event(static_cast<sentry_level_t>(event.level), event.title.c_str(), event.message.c_str()));
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
        it->second.lastCapture = system_clock::now();
        it->second.lastUpload = system_clock::now();
        _events.try_emplace(event.getStr(), event);
        return;
    }

    auto &storedEvent = it->second;
    storedEvent.captureCount++;

    if (eventLastCaptureIsOld(storedEvent)) {  // Reset the capture count if the last capture was more than 10 minutes ago
        storedEvent.captureCount = 0;
        storedEvent.lastCapture = system_clock::now();
        return;
    }

    storedEvent.lastCapture = system_clock::now();
    if (storedEvent.captureCount < SentryMaxCaptureCountBeforeRateLimit) {  // Rate limit not reached, we can send the event
        return;
    }

    if (!eventLastUploadIsOld(storedEvent)) {  // Rate limit reached for this event wait 10 minutes before sending it again
        toUpload = false;
        return;
    }
    escalateErrorLevel(storedEvent);
}

bool SentryHandler::eventLastCaptureIsOld(const SentryEvent &event) const {
    using namespace std::chrono;
    if (event.lastCapture + minutes(SentryMinUploadIntervaOnRateLimit) >= system_clock::now()) {
        return true;
    }
    return false;
}

bool SentryHandler::eventLastUploadIsOld(const SentryEvent &event) const {
    using namespace std::chrono;
    if (event.lastUpload + minutes(SentryMinUploadIntervaOnRateLimit) >= system_clock::now()) {
        return true;
    }
    return false;
}

void SentryHandler::escalateErrorLevel(SentryEvent &event) {
    event.level = SentryLevel::Error;
    event.message += " (Rate limit reached: " + std::to_string(event.captureCount) +
                     " captures since last app start. Level escalated from " + toString(event.level) + " to Error)";
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
    } else {  // Anonymous
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
    }
}

SentryHandler::SentryEvent::SentryEvent(const std::string &title, const std::string &message, SentryLevel level,
                                        SentryConfidentialityLevel confidentialityLevel, const SentryUser &user)
    : title(title), message(message), level(level), confidentialityLevel(confidentialityLevel), userId(user.userId()) {}
}  // namespace KDC