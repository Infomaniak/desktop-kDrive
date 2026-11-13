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

#include "matomoservice.h"

#include "app/appconstants.h"
#include "app/cache/parametersstore.h"
#include "app/clientsettings.h"
#include "config.h"
#include "version.h"

#include <MatomoQt/Event.h>
#include <MatomoQt/PageView.h>
#include <MatomoQt/UserAgentBuilder.h>

#include <QGuiApplication>
#include <QLoggingCategory>
#include <QNetworkReply>
#include <QScreen>

#include <cstdint>

using namespace Qt::StringLiterals;

namespace KDC {

namespace {

Q_LOGGING_CATEGORY(lcMatomoService, "gui.v4.matomo", QtInfoMsg)

// Every tracked user action is a click; the other Matomo event actions are not used by the application.
constexpr char clickEventAction[] = "click";

// Logical size of the primary screen at startup, as browsers report it: the tracker configuration cannot change later.
QString screenResolution() {
    const QScreen *const screen = QGuiApplication::primaryScreen();
    if (screen == nullptr) {
        return {};
    }

    const QSize size = screen->size();
    return u"%1x%2"_s.arg(size.width()).arg(size.height());
}

MatomoQt::TrackerConfig trackerConfig() {
    MatomoQt::TrackerConfig config;
    config.endpoint = AppConstants::Matomo::trackingEndpointUri();
    config.actionUrlBase = QUrl{QString::fromLatin1(AppConstants::Matomo::actionUrlBase)};
    config.siteId = AppConstants::Matomo::siteId;
    config.privacyMode = MatomoQt::PrivacyMode::Value::RequiresConsent;
    config.userAgent = MatomoQt::UserAgentBuilder::buildDesktopUserAgent(MatomoQt::UserAgentBuilder::currentDesktopInfo(
            QString::fromLatin1(APPLICATION_NAME), QString::fromLatin1(KDRIVE_VERSION_STRING)));
    config.screenResolution = screenResolution();
    return config;
}

} // namespace

MatomoService::MatomoService(const ParametersStore &parametersStore, QObject *parent) :
    QObject(parent),
    _parametersStore(parametersStore),
    _settings(ClientSettings::filePath(), QSettings::IniFormat),
    _tracker(trackerConfig(), &_networkAccessManager, _consentStore, _clientIdStore, nullptr) {
    (void) connect(&_parametersStore, &ParametersStore::parametersChanged, this,
                   &MatomoService::reconcileConsentWithParametersStore);
    (void) connect(&_tracker, &MatomoQt::Tracker::dispatchFinished, this, &MatomoService::logDispatchResult);
}

void MatomoService::trackPageView(const QString &category) {
    const MatomoQt::PageView pageView{.path = category, .actionName = category, .customDimensions = {}};

    if (const auto result = _tracker.trackPageView(pageView); !result.accepted()) {
        logRejectedRequest(result, category, QString());
        return;
    }

    qCDebug(lcMatomoService) << "Matomo page view queued | category:" << category;
}

void MatomoService::trackEvent(const QString &category, const QString &eventName, const std::optional<double> value) {
    const MatomoQt::Event event{.category = category,
                                .action = QString::fromLatin1(clickEventAction),
                                .name = eventName,
                                .value = value,
                                .customDimensions = {},
                                .path = category};

    if (const auto result = _tracker.trackEvent(event); !result.accepted()) {
        logRejectedRequest(result, category, eventName);
        return;
    }

    qCDebug(lcMatomoService) << "Matomo event queued | category:" << category << "/ name:" << eventName
                             << "/ value:" << (value.has_value() ? QString::number(*value) : u"none"_s);
}

void MatomoService::reconcileConsentWithParametersStore() {
    const auto parameters = _parametersStore.parameters();
    if (!parameters.has_value()) {
        return;
    }

    const bool enabled = parameters->matomoEnabled();
    if (_appliedConsent == enabled) {
        return;
    }

    using enum MatomoQt::ConsentState::Value;

    if (enabled) {
        _tracker.setConsentState(Granted);
    } else {
        // An opt-out after a granted consent is a withdrawal; at startup it is a plain denial. Both forget the visitor id.
        _tracker.setConsentState(_appliedConsent.has_value() ? Withdrawn : Denied);
        abortPendingRequests();
    }

    _appliedConsent = enabled;
    qCInfo(lcMatomoService) << "Matomo consent applied | enabled:" << enabled;
}

// The SDK does not cancel its requests when the consent is withdrawn: a request sent before, still in flight on a slow
// connection, would reach Matomo with the forgotten visitor id. Its replies are children of the network manager, and
// the SDK reports an aborted reply as cancelled, not as a server failure.
void MatomoService::abortPendingRequests() {
    uint32_t abortedCount = 0;
    for (auto *const reply: _networkAccessManager.findChildren<QNetworkReply *>(Qt::FindDirectChildrenOnly)) {
        if (reply->isRunning()) {
            reply->abort();
            ++abortedCount;
        }
    }

    if (abortedCount > 0) {
        qCInfo(lcMatomoService) << "Matomo requests aborted after the consent withdrawal | count:" << abortedCount;
    }
}

/**
 * Logs a tracking call refused before any network request. Privacy refusals are expected (consent not loaded or denied)
 * and stay silent; the other statuses reveal an invalid configuration or payload.
 */
void MatomoService::logRejectedRequest(const MatomoQt::RequestResult &result, const QString &category,
                                       const QString &eventName) const {
    using enum MatomoQt::RequestStatus::Value;

    if (result.status == RequestBlockedByPrivacy || result.status == RequestDisabled) {
        return;
    }

    qCWarning(lcMatomoService) << "Matomo tracking request rejected | status:" << result.status << "/ category:" << category
                               << "/ name:" << eventName << "/ message:" << result.message;
}

/**
 * Logs network failures once per failure streak: while offline, every tracked action would otherwise add a line.
 */
void MatomoService::logDispatchResult(const MatomoQt::DispatchResult &result) {
    if (result.success()) {
        if (_lastDispatchFailed) {
            qCInfo(lcMatomoService) << "Matomo dispatch recovered";
        }

        _lastDispatchFailed = false;
        return;
    }

    if (!_lastDispatchFailed) {
        qCInfo(lcMatomoService) << "Matomo dispatch failed | status:" << result.status << "/ httpStatus:" << result.httpStatus;
    }

    _lastDispatchFailed = true;
}

} // namespace KDC
