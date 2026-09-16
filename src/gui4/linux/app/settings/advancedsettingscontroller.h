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

#include "app/services/parametersservice.h"

#include <QObject>
#include <QUrl>

#include <array>
#include <cstddef>
#include <cstdint>

namespace KDC {

class ParametersStore;
class SentryService;
class TranslationService;

/** Confirmed advanced preferences, page-scoped errors, and immediate-save actions for the Advanced detail pages. */
class AdvancedSettingsController final : public QObject {
        Q_OBJECT
        Q_PROPERTY(bool ready READ ready NOTIFY changed)
        Q_PROPERTY(bool saving READ saving NOTIFY changed)
        Q_PROPERTY(bool matomoEnabled READ matomoEnabled NOTIFY changed)
        Q_PROPERTY(bool sentryEnabled READ sentryEnabled NOTIFY changed)
        Q_PROPERTY(QString dataManagementErrorText READ dataManagementErrorText NOTIFY changed)
        Q_PROPERTY(QString matomoErrorText READ matomoErrorText NOTIFY changed)
        Q_PROPERTY(QString sentryErrorText READ sentryErrorText NOTIFY changed)

    public:
        AdvancedSettingsController(ParametersStore &parametersStore, ParametersService &parametersService,
                                   SentryService &sentryService, const TranslationService &translationService,
                                   QObject *parent = nullptr);

        [[nodiscard]] bool ready() const;
        [[nodiscard]] bool saving() const { return _saving; }
        [[nodiscard]] bool matomoEnabled() const;
        [[nodiscard]] bool sentryEnabled() const;
        [[nodiscard]] QString dataManagementErrorText() const;
        [[nodiscard]] QString matomoErrorText() const;
        [[nodiscard]] QString sentryErrorText() const;

        Q_INVOKABLE void setMatomoEnabled(bool enabled);
        Q_INVOKABLE void setSentryEnabled(bool enabled);
        Q_INVOKABLE void openSources();

    signals:
        void changed();

    private:
        enum class Error : uint8_t {
            None,
            Save,
            OpenUrl,
        };

        enum class ErrorContext : uint8_t {
            DataManagement,
            Matomo,
            Sentry,
            Count,
        };

        struct ErrorState {
                Error error{Error::None};
                QUrl failedUrl;
        };

        [[nodiscard]] QString errorText(ErrorContext context) const;
        void setError(ErrorContext context, Error error, const QUrl &failedUrl = {});
        void beginSave(ErrorContext context);
        void finishSave(const ExitInfo &result, ErrorContext context);

        ParametersStore &_parametersStore;
        ParametersService &_parametersService;
        SentryService &_sentryService;
        bool _saving{false};
        std::array<ErrorState, static_cast<std::size_t>(ErrorContext::Count)> _errors;
};

} // namespace KDC
