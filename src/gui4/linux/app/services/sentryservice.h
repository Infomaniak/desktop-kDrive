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

#include "libcommon/info/parametersinfo.h"

#include <QObject>
#include <QString>

#include <optional>
#include <string>

namespace KDC {

class AppCache;
class CommService;

/**
 * Linux v4 Sentry coordinator.
 *
 * Role: own Linux v4 Sentry consent reconciliation, delayed initialization, user identity, and UI/process capture helpers.
 * Non-role: decide server-side persistence; the server remains the source of truth for ParametersInfo.
 */
class SentryService final : public QObject {
        Q_OBJECT

    public:
        explicit SentryService(CommService &commService, AppCache &appCache, QObject *parent = nullptr);

        [[nodiscard]] static std::optional<bool> readCachedConsent();
        static void writeCachedConsent(bool enabled);
        static void initializeFromCachedConsent();
        static void initializeWithLinuxConfig();
        [[nodiscard]] static bool isInitialized();

        static void reportError(const std::string &title, const std::string &message);
        static void reportError(const QString &title, const QString &message);
        [[noreturn]] static void reportFatalAndExit(const std::string &title, const std::string &message);

        void reconcileConsentWithServer();
        void setConsentEnabled(bool enabled);
        void updateAuthenticatedUser() const;

    private:
        void applyConsent(bool enabled);
        void setCurrentParametersInfo(const ParametersInfo &parametersInfo);

        CommService &_commService;
        AppCache &_appCache;
        std::optional<ParametersInfo> _currentParametersInfo;
        bool _sentryInitialized{false};
};

} // namespace KDC
