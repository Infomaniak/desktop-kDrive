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

#include "app/services/commservice.h"
#include "app/services/parametersservice.h"

#include <QObject>
#include <QUrl>
#include <QVariantList>

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
        Q_PROPERTY(bool useLog READ useLog NOTIFY changed)
        Q_PROPERTY(bool purgeOldLogs READ purgeOldLogs NOTIFY changed)
        Q_PROPERTY(bool extendedLog READ extendedLog NOTIFY changed)
        Q_PROPERTY(bool debugLevelEnabled READ debugLevelEnabled NOTIFY changed)
        Q_PROPERTY(int32_t logLevel READ logLevel NOTIFY changed)
        Q_PROPERTY(QVariantList logLevels READ logLevels NOTIFY changed)
        Q_PROPERTY(int32_t uploadState READ uploadState NOTIFY changed)
        Q_PROPERTY(int32_t uploadPercentage READ uploadPercentage NOTIFY changed)
        Q_PROPERTY(bool uploadInProgress READ uploadInProgress NOTIFY changed)
        Q_PROPERTY(bool uploadCancellationPending READ uploadCancellationPending NOTIFY changed)
        Q_PROPERTY(bool uploadHasResult READ uploadHasResult NOTIFY changed)
        Q_PROPERTY(bool uploadSucceeded READ uploadSucceeded NOTIFY changed)
        Q_PROPERTY(bool lastUploadFailed READ lastUploadFailed NOTIFY changed)
        Q_PROPERTY(QString uploadStatusText READ uploadStatusText NOTIFY changed)
        Q_PROPERTY(QString dataManagementErrorText READ dataManagementErrorText NOTIFY changed)
        Q_PROPERTY(QString matomoErrorText READ matomoErrorText NOTIFY changed)
        Q_PROPERTY(QString sentryErrorText READ sentryErrorText NOTIFY changed)
        Q_PROPERTY(QString debugErrorText READ debugErrorText NOTIFY changed)

    public:
        AdvancedSettingsController(ParametersStore &parametersStore, ParametersService &parametersService,
                                   SentryService &sentryService, const CommService &commService,
                                   const TranslationService &translationService, QObject *parent = nullptr);

        [[nodiscard]] bool ready() const;
        [[nodiscard]] bool saving() const { return _saving; }
        [[nodiscard]] bool matomoEnabled() const;
        [[nodiscard]] bool sentryEnabled() const;
        [[nodiscard]] bool useLog() const;
        [[nodiscard]] bool purgeOldLogs() const;
        [[nodiscard]] bool extendedLog() const;
        [[nodiscard]] bool debugLevelEnabled() const { return ready() && !extendedLog(); }
        [[nodiscard]] int32_t logLevel() const;
        [[nodiscard]] static QVariantList logLevels();
        [[nodiscard]] int32_t uploadState() const { return static_cast<int32_t>(_uploadState); }
        [[nodiscard]] int32_t uploadPercentage() const { return _uploadPercentage; }
        [[nodiscard]] bool uploadInProgress() const;
        [[nodiscard]] bool uploadCancellationPending() const { return _uploadState == LogUploadState::CancelRequested; }
        [[nodiscard]] bool uploadHasResult() const {
            return _uploadState == LogUploadState::Success || _uploadState == LogUploadState::Failed ||
                   _uploadState == LogUploadState::Canceled;
        }
        [[nodiscard]] bool uploadSucceeded() const { return _uploadState == LogUploadState::Success; }
        [[nodiscard]] bool lastUploadFailed() const { return _uploadState == LogUploadState::Failed; }
        [[nodiscard]] QString uploadStatusText() const;
        [[nodiscard]] QString dataManagementErrorText() const;
        [[nodiscard]] QString matomoErrorText() const;
        [[nodiscard]] QString sentryErrorText() const;
        [[nodiscard]] QString debugErrorText() const;

        Q_INVOKABLE void setMatomoEnabled(bool enabled);
        Q_INVOKABLE void setSentryEnabled(bool enabled);
        Q_INVOKABLE void setUseLog(bool enabled);
        Q_INVOKABLE void setPurgeOldLogs(bool enabled);
        Q_INVOKABLE void setExtendedLog(bool enabled);
        Q_INVOKABLE void setLogLevel(int32_t level);
        Q_INVOKABLE void openSources();
        Q_INVOKABLE void openDebugFolder();
        Q_INVOKABLE void sendDebugLogs(bool lastSessionOnly);
        Q_INVOKABLE void cancelDebugLogs();
        Q_INVOKABLE void resetDebugLogsUploadPresentation();

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
            Debug,
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
        void save(const ParametersService::ParametersMutation &mutation, ErrorContext context);
        void setUploadStatus(LogUploadState state, int32_t percentage);

        ParametersStore &_parametersStore;
        ParametersService &_parametersService;
        SentryService &_sentryService;
        const CommService &_commService;
        bool _saving{false};
        std::array<ErrorState, static_cast<std::size_t>(ErrorContext::Count)> _errors;
        LogUploadState _uploadState{LogUploadState::None};
        int32_t _uploadPercentage{0};
        bool _uploadRequestPending{false};
};

} // namespace KDC
