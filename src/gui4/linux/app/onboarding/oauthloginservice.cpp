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

#include "oauthloginservice.h"

#include "app/services/sentryservice.h"
#include "config.h"
#include "libcommon/utility/urlhelper.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QLoggingCategory>
#include <QUrlQuery>

#include <openssl/err.h>
#include <openssl/rand.h>

#include <array>
#include <cstdint>
#include <vector>

namespace {
constexpr char responseTypeKey[] = "response_type";
constexpr char clientIdKey[] = "client_id";
constexpr char redirectUriKey[] = "redirect_uri";
constexpr char codeChallengeMethodKey[] = "code_challenge_method";
constexpr char codeChallengeKey[] = "code_challenge";
constexpr char stateKey[] = "state";
constexpr char skipAutoRedirectKey[] = "skipAutoRedirect";

constexpr char hashMode[] = "S256";
constexpr char responseType[] = "code";
constexpr char authorizePath[] = "/authorize";
// 128 bits for the CSRF state, and 256 bits for the PKCE code_verifier, which gives the 43 characters recommended by RFC 7636.
constexpr int32_t stateRandomByteCount = 16;
constexpr int32_t codeVerifierRandomByteCount = 32;

constexpr char browserOpenFailed[] = "browser_open_failed";
constexpr char authorizationCallbackInvalid[] = "authorization_callback_invalid";
constexpr char secureRandomGenerationFailed[] = "secure_random_generation_failed";
} // namespace

namespace KDC {

namespace {
Q_LOGGING_CATEGORY(lcOAuthLoginService, "gui.v4.oauthlogin", QtInfoMsg)

/**
 * Draws byteCount bytes from the OpenSSL CSPRNG and encodes them in base64url without padding.
 * Returns false, after logging and reporting the OpenSSL error, if the generator fails.
 */
bool generateSecureRandomBase64Url(const int32_t byteCount, QString &value) {
    std::vector<unsigned char> bytes(static_cast<size_t>(byteCount));
    if (RAND_bytes(bytes.data(), byteCount) != 1) {
        std::array<char, 256> reason{};
        ERR_error_string_n(ERR_get_error(), reason.data(), reason.size());
        qCWarning(lcOAuthLoginService) << "Failed to generate OAuth secure random value | reason:" << reason.data();
        SentryService::reportError(QStringLiteral("Failed to generate OAuth secure random value"),
                                   QStringLiteral("RAND_bytes failed: %1").arg(QString::fromLatin1(reason.data())));
        return false;
    }

    value = QString::fromLatin1(
            QByteArrayView(bytes).toByteArray().toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
    return true;
}
} // namespace

OAuthLoginService::OAuthLoginService(QObject *const parent) :
    QObject(parent) {}

void OAuthLoginService::startAuthorization() {
    if (!_authorizationActive && !beginAuthorization()) {
        emit authorizationFailed(QString::fromLatin1(secureRandomGenerationFailed), QString());
        return;
    }

    if (!QDesktopServices::openUrl(_authorizationUrl)) {
        qCWarning(lcOAuthLoginService) << "Failed to open OAuth authorization URL in browser";
        SentryService::reportError(QStringLiteral("Failed to open OAuth authorization URL in browser"),
                                   QStringLiteral("QDesktopServices::openUrl returned false"));
        resetAuthorization();
        emit authorizationFailed(QString::fromLatin1(browserOpenFailed), QString());
        return;
    }

    qCInfo(lcOAuthLoginService) << "OAuth authorization browser launch requested";
}

void OAuthLoginService::handleAuthorizationCode(const QString &code, const QString &state) {
    if (!_authorizationActive) {
        qCInfo(lcOAuthLoginService) << "Stale OAuth authorization callback ignored";
        return;
    }

    if (code.isEmpty() || state.isEmpty() || state != _state) {
        qCWarning(lcOAuthLoginService) << "Invalid OAuth authorization callback ignored";
        resetAuthorization();
        emit authorizationFailed(QString::fromLatin1(authorizationCallbackInvalid), QString());
        return;
    }

    const auto codeVerifier = _codeVerifier;
    resetAuthorization();

    qCInfo(lcOAuthLoginService) << "OAuth authorization callback accepted";
    emit authorizationCodeReady(code, codeVerifier);
}

bool OAuthLoginService::beginAuthorization() {
    QString state;
    QString codeVerifier;
    if (!generateSecureRandomBase64Url(stateRandomByteCount, state) ||
        !generateSecureRandomBase64Url(codeVerifierRandomByteCount, codeVerifier)) {
        return false;
    }

    _state = state;
    _codeVerifier = codeVerifier;
    _authorizationUrl = generateAuthorizeUrl();
    _authorizationActive = true;
    return true;
}

void OAuthLoginService::resetAuthorization() {
    _authorizationActive = false;
    _authorizationUrl = QUrl{};
    _state.clear();
    _codeVerifier.clear();
}

QUrl OAuthLoginService::generateAuthorizeUrl() const {
    QUrl url(QString::fromStdString(UrlHelper::loginApiUrl()));
    url.setPath(QString::fromLatin1(authorizePath));

    const QString codeChallenge = generateCodeChallenge(_codeVerifier);

    QUrlQuery query;
    query.addQueryItem(QString::fromLatin1(responseTypeKey), QString::fromLatin1(responseType));
    query.addQueryItem(QString::fromLatin1(clientIdKey), QStringLiteral(CLIENT_ID));
    query.addQueryItem(QString::fromLatin1(redirectUriKey), QStringLiteral(REDIRECT_URI));
    query.addQueryItem(QString::fromLatin1(codeChallengeMethodKey), QString::fromLatin1(hashMode));
    query.addQueryItem(QString::fromLatin1(codeChallengeKey), codeChallenge);
    query.addQueryItem(QString::fromLatin1(stateKey), _state);
    query.addQueryItem(QString::fromLatin1(skipAutoRedirectKey), QStringLiteral("true"));

    url.setQuery(query);
    return url;
}

QString OAuthLoginService::generateCodeChallenge(const QString &codeVerifier) const {
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(codeVerifier.toUtf8());
    return hash.result().toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

} // namespace KDC
