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

#include "app/cache/appcache.h"
#include "app/services/commservice.h"
#include "config.h"
#include "libcommon/log/sentry/handler.h"

#include <QByteArray>
#include <QLoggingCategory>
#include <QSettings>

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
    _appCache(appCache) {}

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

    if (settings.status() != QSettings::NoError) {
        qCWarning(lcSentryService) << "Failed to persist Sentry cached consent | enabled:" << enabled
                                   << "/ status:" << settings.status();
        return;
    }

    qCInfo(lcSentryService) << "Sentry cached consent persisted | enabled:" << enabled;
}

void SentryService::initializeFromCachedConsent() {
    const auto cachedConsent = readCachedConsent();
    qCInfo(lcSentryService) << "Sentry cached consent read | known:" << cachedConsent.has_value()
                            << "/ enabled:" << cachedConsent.value_or(false);
    if (cachedConsent.value_or(false)) {
        initializeWithLinuxConfig();
        return;
    }

    qCInfo(lcSentryService) << "Sentry early init skipped because cached consent is not enabled";
}

void SentryService::initializeWithLinuxConfig() {
    if (sentry::Handler::isInitialized()) {
        qCInfo(lcSentryService) << "Sentry already initialized; activating handler";
        sentry::Handler::instance()->setIsSentryActivated(true);
        return;
    }

    qCInfo(lcSentryService) << "Initializing Sentry with Linux v4 configuration";
    sentry::Handler::init(AppType::Client, sentry::Handler::defaultBreadcrumbsSize, SENTRY_CLIENT_LINUX_DSN,
                          SENTRY_CLIENT_LINUX_DB_PATH);
    if (!sentry::Handler::isInitialized()) {
        qCWarning(lcSentryService) << "Sentry initialization completed without an active handler";
        return;
    }

    sentry::Handler::instance()->setGlobalConfidentialityLevel(sentry::ConfidentialityLevel::Authenticated);
    sentry::Handler::instance()->setIsSentryActivated(true);
    qCInfo(lcSentryService) << "Sentry initialized and activated";
}

bool SentryService::isInitialized() {
    return sentry::Handler::isInitialized();
}

void SentryService::reportError(const std::string &title, const std::string &message) {
    sentry::Handler::captureMessage(sentry::Level::Error, title, message);
}

void SentryService::reportError(const QString &title, const QString &message) {
    reportError(qStringToUtf8String(title), qStringToUtf8String(message));
}

void SentryService::reportFatalAndExit(const std::string &title, const std::string &message) {
    if (isInitialized()) {
        sentry::Handler::captureMessage(sentry::Level::Fatal, title, message);
        sentry::Handler::shutdown();
    }
    std::exit(EXIT_FAILURE);
}

void SentryService::reportFatalAndExit(const char *title, const char *message) {
    reportFatalAndExit(std::string(title), std::string(message));
}

void SentryService::reportFatalAndExit(const QString &title, const QString &message) {
    reportFatalAndExit(qStringToUtf8String(title), qStringToUtf8String(message));
}

void SentryService::reconcileConsentWithServer() {
    qCInfo(lcSentryService) << "Reconciling Sentry consent with server";
    _commService.requestParametersInfo([this](const ExitInfo &exitInfo, const ParametersInfo &parametersInfo) {
        if (!exitInfo) {
            qCWarning(lcSentryService) << "Sentry consent reconciliation failed | ExitInfo:"
                                       << QString::fromStdString(toString(exitInfo));
            return;
        }

        qCInfo(lcSentryService) << "Sentry consent reconciled with server | enabled:" << parametersInfo.sentryEnabled();
        setCurrentParametersInfo(parametersInfo);
        writeCachedConsent(parametersInfo.sentryEnabled());
        applyConsent(parametersInfo.sentryEnabled());
    });
}

void SentryService::setConsentEnabled(const bool enabled) {
    qCInfo(lcSentryService) << "Sentry consent update requested | enabled:" << enabled;
    if (!_currentParametersInfo.has_value()) {
        qCWarning(lcSentryService) << "Sentry consent update ignored because server parameters are not loaded yet";
        return;
    }

    ParametersInfo updatedParametersInfo = *_currentParametersInfo;
    updatedParametersInfo.setSentryEnabled(enabled);
    _commService.requestParametersUpdate(updatedParametersInfo, [this, updatedParametersInfo, enabled](const ExitInfo &exitInfo) {
        if (!exitInfo) {
            qCWarning(lcSentryService) << "Sentry consent update failed | ExitInfo:"
                                       << QString::fromStdString(toString(exitInfo));
            return;
        }

        qCInfo(lcSentryService) << "Sentry consent update confirmed by server | enabled:" << enabled;
        setCurrentParametersInfo(updatedParametersInfo);
        writeCachedConsent(enabled);
        applyConsent(enabled);
    });
}

void SentryService::updateAuthenticatedUser() const {
    if (!isInitialized()) {
        qCDebug(lcSentryService) << "Sentry user update skipped because handler is not initialized";
        return;
    }

    const auto users = _appCache.users();
    const auto selectedUserIt = std::ranges::find_if(users, [](const UserInfo &user) { return user.connected(); });
    const auto userIt = selectedUserIt != users.end() ? selectedUserIt : users.begin();
    if (userIt == users.end()) {
        sentry::Handler::instance()->setAuthenticatedUser(SentryUser("No user logged", "No user logged", "0"));
        qCInfo(lcSentryService) << "Sentry user set to anonymous fallback";
        return;
    }

    sentry::Handler::instance()->setAuthenticatedUser(SentryUser(
            qStringToUtf8String(userIt->email()), qStringToUtf8String(userIt->name()), std::to_string(userIt->userId())));
    qCInfo(lcSentryService) << "Sentry authenticated user updated | userId:" << userIt->userId()
                            << "/ connected:" << userIt->connected();
}

void SentryService::applyConsent(const bool enabled) {
    qCInfo(lcSentryService) << "Applying Sentry consent | enabled:" << enabled << "/ initialized:" << isInitialized();
    if (enabled) {
        if (!isInitialized()) {
            initializeWithLinuxConfig();
            qCInfo(lcSentryService) << "Sentry deferred initialization result | initialized:" << isInitialized();
        } else if (isInitialized()) {
            sentry::Handler::instance()->setIsSentryActivated(true);
            qCInfo(lcSentryService) << "Sentry handler activated after consent update";
        }
        updateAuthenticatedUser();
        return;
    }

    if (isInitialized()) {
        qCInfo(lcSentryService) << "Shutting down Sentry after consent opt-out";
        sentry::Handler::shutdown();
        return;
    }

    qCInfo(lcSentryService) << "Sentry shutdown skipped because handler is not initialized";
}

void SentryService::setCurrentParametersInfo(const ParametersInfo &parametersInfo) {
    _currentParametersInfo = parametersInfo;
}

} // namespace KDC
