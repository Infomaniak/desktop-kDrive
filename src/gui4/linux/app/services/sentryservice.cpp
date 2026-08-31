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
#include "app/cache/parametersstore.h"
#include "app/services/parametersservice.h"
#include "config.h"
#include "libcommon/log/sentry/handler.h"
#include "libcommon/utility/utility.h"

#include <QGuiApplication>
#include <QLoggingCategory>
#include <QSettings>

#include <algorithm>
#include <cstdlib>

using namespace Qt::StringLiterals;

namespace {

Q_LOGGING_CATEGORY(lcSentryService, "gui.v4.sentry", QtInfoMsg)

constexpr char settingsOrganization[] = "Infomaniak";
constexpr char settingsApplication[] = APPLICATION_NAME;
constexpr char sentryConsentKey[] = "sentry/enabled";

QString normalizedDisplayServer(const QString &platformPlugin) {
    if (platformPlugin.contains(QStringLiteral("wayland"), Qt::CaseInsensitive)) {
        return QStringLiteral("wayland");
    }
    if (platformPlugin.compare(QStringLiteral("xcb"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("x11");
    }
    return QStringLiteral("unknown");
}

QString normalizedDesktopEnvironment(const QString &desktopEnvironment) {
    const QString normalized = desktopEnvironment.toLower();
    if (normalized.contains(QStringLiteral("gnome"))) return QStringLiteral("gnome");
    if (normalized.contains(QStringLiteral("kde")) || normalized.contains(QStringLiteral("plasma"))) {
        return QStringLiteral("kde");
    }
    if (normalized.contains(QStringLiteral("cinnamon"))) return QStringLiteral("cinnamon");
    if (normalized.contains(QStringLiteral("xfce"))) return QStringLiteral("xfce");
    if (normalized.contains(QStringLiteral("mate"))) return QStringLiteral("mate");
    if (normalized.contains(QStringLiteral("lxqt"))) return QStringLiteral("lxqt");
    if (normalized.contains(QStringLiteral("lxde"))) return QStringLiteral("lxde");
    return normalized.isEmpty() ? QStringLiteral("unknown") : QStringLiteral("other");
}
} // namespace

namespace KDC {

SentryService::SentryService(ParametersService &parametersService, AppCache &appCache, ParametersStore &parametersStore,
                             QObject *const parent) :
    QObject(parent),
    _parametersService(parametersService),
    _appCache(appCache),
    _parametersStore(parametersStore) {
    (void) connect(&_parametersStore, &ParametersStore::parametersInfoChanged, this,
                   &SentryService::reconcileConsentWithParametersStore);
    updateLinuxRuntimeTags();
}

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
        updateLinuxRuntimeTags();
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
    updateLinuxRuntimeTags();
    qCInfo(lcSentryService) << "Sentry initialized and activated";
}

void SentryService::updateLinuxRuntimeTags() {
    if (!isInitialized() || qobject_cast<QGuiApplication *>(QCoreApplication::instance()) == nullptr) {
        return;
    }

    const QString platformPlugin = QGuiApplication::platformName().toLower();
    const QString desktopEnvironment = qEnvironmentVariable("XDG_CURRENT_DESKTOP");
    const auto handler = sentry::Handler::instance();
    handler->setTag("linux.distribution", CommonUtility::platformName().toStdString());
    handler->setTag("linux.distribution_version", CommonUtility::osVersion());
    handler->setTag("os.display_server", normalizedDisplayServer(platformPlugin).toStdString());
    handler->setTag("os.desktop", normalizedDesktopEnvironment(desktopEnvironment).toStdString());
    handler->setTag("os.packaging", qEnvironmentVariable("APPIMAGE").isEmpty() ? "other" : "appimage");
    handler->setTag("qt.version", qVersion());
    handler->setTag("qt.platform_plugin", platformPlugin.toStdString());
}

bool SentryService::isInitialized() {
    return sentry::Handler::isInitialized();
}

void SentryService::shutdown() {
    if (!isInitialized()) {
        qCInfo(lcSentryService) << "Sentry shutdown skipped because handler is not initialized";
        return;
    }

    qCInfo(lcSentryService) << "Shutting down Sentry";
    sentry::Handler::shutdown();
}

void SentryService::reportError(const std::string &title, const std::string &message) {
    sentry::Handler::captureMessage(sentry::Level::Error, title, message);
}

void SentryService::reportError(const QString &title, const QString &message) {
    reportError(title.toStdString(), message.toStdString());
}

void SentryService::reportFatalAndExit(const std::string &title, const std::string &message) {
    if (isInitialized()) {
        sentry::Handler::captureMessage(sentry::Level::Fatal, title, message);
        sentry::Handler::shutdown();
    }
    qCCritical(lcSentryService) << "Fatal error occurred, exiting application | title:" << QString::fromStdString(title)
                                << "/ message:" << QString::fromStdString(message);
    std::exit(EXIT_FAILURE);
}

void SentryService::reportFatalAndExit(const char *title, const char *message) {
    reportFatalAndExit(std::string(title), std::string(message));
}

void SentryService::reportFatalAndExit(const QString &title, const QString &message) {
    reportFatalAndExit(title.toStdString(), message.toStdString());
}

void SentryService::setConsent(const bool enabled) const {
    qCInfo(lcSentryService) << "Sentry consent update requested | enabled:" << enabled;

    const ParametersService::ParametersMutation mutation = [enabled](ParametersInfo &parametersInfo) {
        parametersInfo.setSentryEnabled(enabled);
    };

    const ParametersService::UpdateCallback callback = [enabled](const ExitInfo &exitInfo) {
        if (!exitInfo) {
            qCWarning(lcSentryService) << "Sentry consent update failed; keeping confirmed store value | ExitInfo:"
                                       << QString::fromStdString(toString(exitInfo));
            return;
        }

        qCInfo(lcSentryService) << "Sentry consent update confirmed by server | enabled:" << enabled;
    };


    _parametersService.updateParameters(mutation, callback);
}

void SentryService::updateAuthenticatedUser() const {
    if (!isInitialized()) {
        qCDebug(lcSentryService) << "Sentry user update skipped because handler is not initialized";
        return;
    }

    const auto users = _appCache.users();
    const auto userIt = std::ranges::find_if(users, [](const User &user) { return user.connected(); });
    if (userIt == users.end()) {
        sentry::Handler::instance()->setGlobalConfidentialityLevel(sentry::ConfidentialityLevel::Anonymous);
        sentry::Handler::instance()->setAuthenticatedUser(SentryUser());
        qCInfo(lcSentryService) << "Sentry user set to anonymous because no connected user is available";
        return;
    }

    sentry::Handler::instance()->setGlobalConfidentialityLevel(sentry::ConfidentialityLevel::Authenticated);
    sentry::Handler::instance()->setAuthenticatedUser(
            SentryUser(userIt->email(), userIt->name(), std::to_string(userIt->userId())));
    qCInfo(lcSentryService) << "Sentry authenticated user updated | userId:" << userIt->userId()
                            << "/ connected:" << userIt->connected();
}

void SentryService::applyConsent(const bool enabled) {
    const bool initialized = isInitialized();
    qCInfo(lcSentryService) << "Applying Sentry consent | enabled:" << enabled << "/ initialized:" << initialized;
    if (enabled) {
        if (!initialized) {
            initializeWithLinuxConfig();
            const bool initializedAfterInit = isInitialized();
            qCInfo(lcSentryService) << "Sentry deferred initialization result | initialized:" << initializedAfterInit;
            if (!initializedAfterInit) {
                return;
            }
        } else {
            sentry::Handler::instance()->setIsSentryActivated(true);
            qCInfo(lcSentryService) << "Sentry handler activated after consent update";
        }
        updateAuthenticatedUser();
        return;
    }

    if (initialized) {
        qCInfo(lcSentryService) << "Shutting down Sentry after consent opt-out";
        sentry::Handler::shutdown();
        return;
    }

    qCInfo(lcSentryService) << "Sentry shutdown skipped because handler is not initialized";
}

void SentryService::reconcileConsentWithParametersStore() {
    const auto currentParametersInfo = _parametersStore.parametersInfo();
    if (!currentParametersInfo.has_value()) {
        qCInfo(lcSentryService) << "Sentry consent reconciliation skipped because parameters are not loaded";
        return;
    }

    qCInfo(lcSentryService) << "Sentry consent reconciled with parameters store | enabled:"
                            << currentParametersInfo->sentryEnabled();
    writeCachedConsent(currentParametersInfo->sentryEnabled());
    applyConsent(currentParametersInfo->sentryEnabled());
    if (isInitialized()) {
        sentry::Handler::instance()->setDistributionChannel(currentParametersInfo->distributionChannel());
    }
}

} // namespace KDC
