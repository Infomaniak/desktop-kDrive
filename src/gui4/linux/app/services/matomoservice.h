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

#pragma once

#include <MatomoQt/InMemoryConsentStore.h>
#include <MatomoQt/QSettingsClientIdStore.h>
#include <MatomoQt/Tracker.h>

#include <QObject>
#include <QSettings>
#include <QString>

#include <optional>

namespace KDC {

class ParametersStore;

/**
 * Linux v4 Matomo coordinator.
 *
 * Role: own the single Matomo tracker, apply the server-confirmed `matomoEnabled` consent, and send page views and events.
 * Nothing is sent before the parameters are loaded, nor once the user opted out; opting out also forgets the visitor id.
 * Non-role: decide what is tracked; `UserActionLog` resolves user actions through `AnalyticsCatalog`.
 */
class MatomoService final : public QObject {
        Q_OBJECT

    public:
        explicit MatomoService(const ParametersStore &parametersStore, QObject *parent = nullptr);

        void trackPageView(const QString &category);
        void trackEvent(const QString &category, const QString &eventName, std::optional<double> value);

    private:
        void reconcileConsentWithParametersStore();
        void logRejectedRequest(const MatomoQt::RequestResult &result, const QString &category, const QString &eventName) const;
        void logDispatchResult(const MatomoQt::DispatchResult &result);

        const ParametersStore &_parametersStore;
        QSettings _settings;
        MatomoQt::InMemoryConsentStore _consentStore;
        MatomoQt::QSettingsClientIdStore _clientIdStore{&_settings};
        MatomoQt::Tracker _tracker;
        std::optional<bool> _appliedConsent;
        bool _lastDispatchFailed{false};
};

} // namespace KDC
