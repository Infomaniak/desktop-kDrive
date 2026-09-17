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

#include "advancedsettingscontroller.h"

#include "app/appconstants.h"
#include "app/cache/parametersstore.h"
#include "app/services/sentryservice.h"
#include "app/services/translationservice.h"

#include <QDesktopServices>
#include <QLoggingCategory>
#include <QPointer>

namespace KDC {

namespace {
Q_LOGGING_CATEGORY(lcAdvancedSettings, "gui.v4.settings.advanced", QtInfoMsg)
}

AdvancedSettingsController::AdvancedSettingsController(ParametersStore &parametersStore, ParametersService &parametersService,
                                                       SentryService &sentryService, const TranslationService &translationService,
                                                       QObject *const parent) :
    QObject(parent),
    _parametersStore(parametersStore),
    _parametersService(parametersService),
    _sentryService(sentryService) {
    (void) connect(&_parametersStore, &ParametersStore::parametersInfoChanged, this, &AdvancedSettingsController::changed);
    (void) connect(&translationService, &TranslationService::languageChanged, this, &AdvancedSettingsController::changed);
}

bool AdvancedSettingsController::ready() const {
    return _parametersStore.parametersInfo().has_value();
}

bool AdvancedSettingsController::matomoEnabled() const {
    const auto parameters = _parametersStore.parametersInfo();
    return parameters && parameters->matomoEnabled();
}

bool AdvancedSettingsController::sentryEnabled() const {
    const auto parameters = _parametersStore.parametersInfo();
    return parameters && parameters->sentryEnabled();
}

QString AdvancedSettingsController::dataManagementErrorText() const {
    return errorText(ErrorContext::DataManagement);
}

QString AdvancedSettingsController::matomoErrorText() const {
    return errorText(ErrorContext::Matomo);
}

QString AdvancedSettingsController::sentryErrorText() const {
    return errorText(ErrorContext::Sentry);
}

QString AdvancedSettingsController::errorText(const ErrorContext context) const {
    switch (const auto &[error, failedUrl] = _errors[static_cast<std::size_t>(context)]; error) {
        case Error::Save:
            return qtTrId("linuxSettingsSaveError");
        case Error::OpenUrl:
            return failedUrl.isEmpty() ? qtTrId("defaultErrorTitle")
                                       : qtTrId("errorOpeningLocalURL").arg(failedUrl.toDisplayString());
        case Error::None:
            return {};
    }
    return {};
}

void AdvancedSettingsController::setMatomoEnabled(const bool enabled) {
    if (!ready() || _saving || enabled == matomoEnabled()) {
        return;
    }
    beginSave(ErrorContext::Matomo);
    _parametersService.updateParameters([enabled](ParametersInfo &parameters) { parameters.setMatomoEnabled(enabled); },
                                        [self = QPointer(this)](const ExitInfo &result) {
                                            if (self) {
                                                self->finishSave(result, ErrorContext::Matomo);
                                            }
                                        });
}

void AdvancedSettingsController::setSentryEnabled(const bool enabled) {
    if (!ready() || _saving || enabled == sentryEnabled()) {
        return;
    }
    beginSave(ErrorContext::Sentry);
    _sentryService.setConsent(enabled, [self = QPointer(this)](const ExitInfo &result) {
        if (self) {
            self->finishSave(result, ErrorContext::Sentry);
        }
    });
}

void AdvancedSettingsController::openSources() {
    if (const auto url = AppConstants::Settings::sourcesUri(); QDesktopServices::openUrl(url)) {
        setError(ErrorContext::DataManagement, Error::None);
    } else {
        qCWarning(lcAdvancedSettings) << "Cannot open source repository URL" << url;
        setError(ErrorContext::DataManagement, Error::OpenUrl, url);
    }
}

void AdvancedSettingsController::setError(const ErrorContext context, const Error error, const QUrl &failedUrl) {
    _errors[static_cast<std::size_t>(context)] = {.error = error, .failedUrl = failedUrl};
    emit changed();
}

void AdvancedSettingsController::beginSave(const ErrorContext context) {
    _saving = true;
    setError(context, Error::None);
}

void AdvancedSettingsController::finishSave(const ExitInfo &result, const ErrorContext context) {
    _saving = false;
    setError(context, result ? Error::None : Error::Save);
}

} // namespace KDC
