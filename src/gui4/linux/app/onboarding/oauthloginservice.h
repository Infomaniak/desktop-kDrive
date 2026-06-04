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

#include <QObject>
#include <QString>
#include <QUrl>

namespace KDC {

/**
 * Owns the Linux v4 OAuth browser launch and callback validation state.
 */
class OAuthLoginService final : public QObject {
        Q_OBJECT

    public:
        explicit OAuthLoginService(QObject *parent = nullptr);

    public slots:
        void startAuthorization();
        void handleAuthorizationCode(const QString &code, const QString &state);

    signals:
        void authorizationCodeReady(const QString &code, const QString &codeVerifier);
        void authorizationFailed(const QString &error, const QString &errorDescription);

    private:
        void beginAuthorization();
        [[nodiscard]] QUrl generateAuthorizeUrl() const;
        [[nodiscard]] QString generateCodeVerifier() const;
        [[nodiscard]] QString generateCodeChallenge(const QString &codeVerifier) const;

        QUrl _authorizationUrl;
        QString _state;
        QString _codeVerifier;
        bool _authorizationActive{false};
};

} // namespace KDC
