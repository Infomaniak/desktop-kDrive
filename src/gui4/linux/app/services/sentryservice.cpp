/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#include "sentryservice.h"

#include "config.h"

#include <QLoggingCategory>
#include <QSettings>
#include <QByteArray>

#include <algorithm>
#include <cstdlib>

namespace {

Q_LOGGING_CATEGORY(lcSentryService, "gui.v4.sentry", QtInfoMsg)

constexpr char settingsOrganization[] = "Infomaniak";
constexpr char settingsApplication[] = APPLICATION_NAME;
constexpr char sentryConsentKey[] = "sentry/enabled";

[[nodiscard]] std::string qStringToUtf8String(const QString &value) {
    const QByteArray utf8 = value.toUtf8();
    return {utf8.constData(), static_cast<size_t>(utf8.size())};
}
} // namespace

namespace KDC {

SentryService::SentryService(CommService &commService, AppCache &appCache, QObject *const parent) :
    QObject(parent),
    _commService(commService),
    _appCache(appCache),
    _sentryInitialized(isInitialized()) {}

std::optional<bool> SentryService::readCachedConsent() {
    const QSettings settings(QSettings::IniFormat, QSettings::UserScope, settingsOrganization, settingsApplication);
    if (!settings.contains(sentryConsentKey)) {
        return std::nullopt;
    }
    return settings.value(sentryConsentKey).toBool();
}

void SentryService::writeCachedConsent(const bool enabled) {
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, settingsOrganization, settingsApplication);
    settings.setValue(sentryConsentKey, enabled);
    settings.sync();
}

void SentryService::initializeFromCachedConsent() {
    if (readCachedConsent().value_or(false)) {
        initializeWithLinuxConfig();
    }
}

void SentryService::initializeWithLinuxConfig() {
    if (sentry::Handler::isInitialized()) {
        sentry::Handler::instance()->setIsSentryActivated(true);
        return;
    }

    sentry::Handler::init(AppType::Client, sentry::Handler::defaultBreadcrumbsSize, SENTRY_CLIENT_LINUX_DSN,
                          SENTRY_CLIENT_LINUX_DB_PATH);
    if (!sentry::Handler::isInitialized()) {
        return;
    }

    sentry::Handler::instance()->setGlobalConfidentialityLevel(sentry::ConfidentialityLevel::Authenticated);
    sentry::Handler::instance()->setIsSentryActivated(true);
}

bool SentryService::isInitialized() {
    return sentry::Handler::isInitialized();
}

void SentryService::reportError(const std::string &title, const std::string &message) {
    if (!isInitialized()) {
        return;
    }
    sentry::Handler::captureMessage(sentry::Level::Error, title, message);
}

void SentryService::reportFatalAndExit(const std::string &title, const std::string &message) {
    if (isInitialized()) {
        sentry::Handler::captureMessage(sentry::Level::Fatal, title, message);
    }
    std::exit(EXIT_FAILURE);
}

void SentryService::reconcileConsentWithServer() {
    _commService.requestParametersInfo([this](const ExitInfo &exitInfo, const ParametersInfo &parametersInfo) {
        if (!exitInfo) {
            qCWarning(lcSentryService) << "Sentry consent reconciliation failed | ExitInfo:"
                                       << QString::fromStdString(toString(exitInfo));
            return;
        }

        setCurrentParametersInfo(parametersInfo);
        writeCachedConsent(parametersInfo.sentryEnabled());
        applyConsent(parametersInfo.sentryEnabled());
    });
}

void SentryService::setConsentEnabled(const bool enabled) {
    ParametersInfo updatedParametersInfo = _currentParametersInfo.value_or(ParametersInfo());
    updatedParametersInfo.setSentryEnabled(enabled);
    _commService.requestParametersUpdate(updatedParametersInfo, [this, updatedParametersInfo, enabled](const ExitInfo &exitInfo) {
        if (!exitInfo) {
            qCWarning(lcSentryService) << "Sentry consent update failed | ExitInfo:"
                                       << QString::fromStdString(toString(exitInfo));
            return;
        }

        setCurrentParametersInfo(updatedParametersInfo);
        writeCachedConsent(enabled);
        applyConsent(enabled);
    });
}

void SentryService::updateAuthenticatedUser() const {
    if (!isInitialized()) {
        return;
    }

    const auto users = _appCache.users();
    const auto selectedUserIt = std::ranges::find_if(users, [](const UserInfo &user) { return user.connected(); });
    const auto userIt = selectedUserIt != users.end() ? selectedUserIt : users.begin();
    if (userIt == users.end()) {
        sentry::Handler::instance()->setAuthenticatedUser(SentryUser("No user logged", "No user logged", "0"));
        return;
    }

    sentry::Handler::instance()->setAuthenticatedUser(SentryUser(
            qStringToUtf8String(userIt->email()), qStringToUtf8String(userIt->name()), std::to_string(userIt->userId())));
}

void SentryService::applyConsent(const bool enabled) {
    if (enabled) {
        if (!_sentryInitialized) {
            initializeWithLinuxConfig();
            _sentryInitialized = isInitialized();
        } else if (isInitialized()) {
            sentry::Handler::instance()->setIsSentryActivated(true);
        }
        updateAuthenticatedUser();
        return;
    }

    if (_sentryInitialized) {
        sentry::Handler::shutdown();
        _sentryInitialized = false;
    }
}

void SentryService::setCurrentParametersInfo(const ParametersInfo &parametersInfo) {
    _currentParametersInfo = parametersInfo;
}

} // namespace KDC
