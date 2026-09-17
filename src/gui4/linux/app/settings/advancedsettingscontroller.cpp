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
#include "libcommon/utility/utility.h"

#include <QDesktopServices>
#include <QLoggingCategory>
#include <QPointer>
#include <QVariantMap>

#include <algorithm>
#include <array>

namespace KDC {

using namespace Qt::StringLiterals;

namespace {
Q_LOGGING_CATEGORY(lcAdvancedSettings, "gui.v4.settings.advanced", QtInfoMsg)
}

AdvancedSettingsController::AdvancedSettingsController(ParametersStore &parametersStore, ParametersService &parametersService,
                                                       SentryService &sentryService, const CommService &commService,
                                                       const TranslationService &translationService, QObject *const parent) :
    QObject(parent),
    _parametersStore(parametersStore),
    _parametersService(parametersService),
    _sentryService(sentryService),
    _commService(commService) {
    (void) connect(&_parametersStore, &ParametersStore::parametersInfoChanged, this, &AdvancedSettingsController::changed);
    (void) connect(&translationService, &TranslationService::languageChanged, this, &AdvancedSettingsController::changed);
    (void) connect(&_commService, &CommService::logUploadStatusUpdated, this,
                   [this](const LogUploadState state, const int32_t percentage) { setUploadStatus(state, percentage); });
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

bool AdvancedSettingsController::useLog() const {
    const auto parameters = _parametersStore.parametersInfo();
    return parameters && parameters->useLog();
}

bool AdvancedSettingsController::purgeOldLogs() const {
    const auto parameters = _parametersStore.parametersInfo();
    return parameters && parameters->purgeOldLogs();
}

bool AdvancedSettingsController::extendedLog() const {
    const auto parameters = _parametersStore.parametersInfo();
    return parameters && parameters->extendedLog();
}

int32_t AdvancedSettingsController::logLevel() const {
    const auto parameters = _parametersStore.parametersInfo();
    return static_cast<int32_t>(parameters ? parameters->logLevel() : LogLevel::Debug);
}

QVariantList AdvancedSettingsController::logLevels() {
    static constexpr std::array ids{"logLevelDebug", "logLevelInfo", "logLevelWarning", "logLevelError", "logLevelFatal"};
    QVariantList values;
    for (int32_t value = static_cast<int32_t>(LogLevel::Debug); value < static_cast<int32_t>(LogLevel::EnumEnd); ++value) {
        values.push_back(QVariantMap{{u"label"_s, qtTrId(ids[static_cast<std::size_t>(value)])}, {u"value"_s, value}});
    }
    return values;
}

bool AdvancedSettingsController::uploadInProgress() const {
    return _uploadRequestPending || _uploadState == LogUploadState::Archiving || _uploadState == LogUploadState::Uploading ||
           _uploadState == LogUploadState::CancelRequested;
}

QString AdvancedSettingsController::uploadStatusText() const {
    // Keep the interrupted phase visible until the server confirms the cancellation.
    switch (_uploadState == LogUploadState::CancelRequested ? _lastUploadPhase : _uploadState) {
        case LogUploadState::Archiving:
            return qtTrId("logsStatusCompression");
        case LogUploadState::Uploading:
            return qtTrId("logsStatusUpload");
        case LogUploadState::Success:
            return qtTrId("logsUploadSuccess");
        case LogUploadState::Failed:
            return qtTrId("logsUploadErrorTooltip");
        case LogUploadState::Canceled:
            return qtTrId("logsUploadCanceled");
        default:
            return {};
    }
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

QString AdvancedSettingsController::debugErrorText() const {
    return errorText(ErrorContext::Debug);
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

void AdvancedSettingsController::setUseLog(const bool enabled) {
    if (enabled != useLog()) {
        save([enabled](ParametersInfo &parameters) { parameters.setUseLog(enabled); }, ErrorContext::Debug);
    }
}

void AdvancedSettingsController::setPurgeOldLogs(const bool enabled) {
    if (enabled != purgeOldLogs()) {
        save([enabled](ParametersInfo &parameters) { parameters.setPurgeOldLogs(enabled); }, ErrorContext::Debug);
    }
}

void AdvancedSettingsController::setExtendedLog(const bool enabled) {
    if (enabled != extendedLog()) {
        save([enabled](ParametersInfo &parameters) { parameters.setExtendedLog(enabled); }, ErrorContext::Debug);
    }
}

void AdvancedSettingsController::setLogLevel(const int32_t level) {
    if (level < static_cast<int32_t>(LogLevel::Debug) || level >= static_cast<int32_t>(LogLevel::EnumEnd) ||
        level == logLevel()) {
        return;
    }
    save([level](ParametersInfo &parameters) { parameters.setLogLevel(static_cast<LogLevel>(level)); }, ErrorContext::Debug);
}

void AdvancedSettingsController::openSources() {
    if (const auto url = AppConstants::Settings::sourcesUri(); QDesktopServices::openUrl(url)) {
        setError(ErrorContext::DataManagement, Error::None);
    } else {
        qCWarning(lcAdvancedSettings) << "Cannot open source repository URL" << url;
        setError(ErrorContext::DataManagement, Error::OpenUrl, url);
    }
}

void AdvancedSettingsController::openDebugFolder() {
    SyncPath path;
    const auto result = CommonUtility::logDirectoryPath(path);
    if (const auto url = result ? QUrl::fromLocalFile(Path2QStr(path)) : QUrl{}; result && QDesktopServices::openUrl(url)) {
        setError(ErrorContext::Debug, Error::None);
    } else {
        qCWarning(lcAdvancedSettings) << "Cannot open debug log folder" << url;
        setError(ErrorContext::Debug, Error::OpenUrl, url);
    }
}

void AdvancedSettingsController::sendDebugLogs(const bool lastSessionOnly) {
    if (uploadInProgress()) return;

    _uploadRequestPending = true;
    // Present the first server phase immediately. The request acknowledgement can arrive before the first asynchronous
    // status signal; keeping Archiving here avoids briefly restoring the idle controls between those two messages.
    _uploadState = LogUploadState::Archiving;
    _lastUploadPhase = LogUploadState::Archiving;
    _uploadPercentage = 0;
    emit changed();

    const bool includeArchivedLogs = !lastSessionOnly;
    _commService.requestSendLogToSupport(includeArchivedLogs, [self = QPointer(this)](const ExitInfo &result) {
        if (!self) return;

        self->_uploadRequestPending = false;
        if (!result) self->_uploadState = LogUploadState::Failed;
        emit self->changed();
    });
}

void AdvancedSettingsController::cancelDebugLogs() {
    if (!uploadInProgress() || _uploadState == LogUploadState::CancelRequested) return;

    _uploadState = LogUploadState::CancelRequested;
    emit changed();

    _commService.requestCancelLogToSupport([self = QPointer(this)](const ExitInfo &result) {
        if (self && !result && self->_uploadState == LogUploadState::CancelRequested) {
            self->_uploadState = LogUploadState::Failed;
            emit self->changed();
        }
    });
}

void AdvancedSettingsController::resetDebugLogsUploadPresentation() {
    if (uploadInProgress() || _uploadState == LogUploadState::None) return;

    _uploadState = LogUploadState::None;
    _uploadPercentage = 0;
    emit changed();
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

void AdvancedSettingsController::save(const ParametersService::ParametersMutation &mutation, const ErrorContext context) {
    if (!ready() || _saving) {
        return;
    }
    beginSave(context);
    _parametersService.updateParameters(mutation, [self = QPointer(this), context](const ExitInfo &result) {
        if (self) {
            self->finishSave(result, context);
        }
    });
}

void AdvancedSettingsController::setUploadStatus(const LogUploadState state, const int32_t percentage) {
    _uploadRequestPending = false;
    _uploadState = state;
    if (state == LogUploadState::Archiving || state == LogUploadState::Uploading) {
        _lastUploadPhase = state;
    }
    _uploadPercentage = std::clamp(percentage, int32_t{0}, int32_t{100});
    emit changed();
}

} // namespace KDC
