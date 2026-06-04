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

#include "config.h"
#include "libcommon/utility/urlhelper.h"
#include "libcommon/utility/utility.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QLoggingCategory>
#include <QUrlQuery>

#include <cstdint>

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
constexpr uint8_t stateStringLength = 8;
constexpr uint8_t codeVerifierRandomStringLength = 32;

constexpr char browserOpenFailed[] = "browser_open_failed";
constexpr char authorizationCallbackInvalid[] = "authorization_callback_invalid";
} // namespace

namespace KDC {

namespace {
Q_LOGGING_CATEGORY(lcOAuthLoginService, "gui.v4.oauthlogin", QtInfoMsg)
} // namespace

OAuthLoginService::OAuthLoginService(QObject *const parent) :
    QObject(parent) {}

void OAuthLoginService::startAuthorization() {
    if (!_authorizationActive) {
        beginAuthorization();
    }

    if (!QDesktopServices::openUrl(_authorizationUrl)) {
        qCWarning(lcOAuthLoginService) << "Failed to open OAuth authorization URL in browser";
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
        emit authorizationFailed(QString::fromLatin1(authorizationCallbackInvalid), QString());
        return;
    }

    const auto codeVerifier = _codeVerifier;
    _authorizationActive = false;
    _authorizationUrl = QUrl{};
    _state.clear();
    _codeVerifier.clear();

    qCInfo(lcOAuthLoginService) << "OAuth authorization callback accepted";
    emit authorizationCodeReady(code, codeVerifier);
}

void OAuthLoginService::beginAuthorization() {
    _state = QString::fromStdString(CommonUtility::generateRandomStringAlphaNum(stateStringLength));
    _codeVerifier = generateCodeVerifier();
    _authorizationUrl = generateAuthorizeUrl();
    _authorizationActive = true;
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

QString OAuthLoginService::generateCodeVerifier() const {
    const std::string str = CommonUtility::generateRandomStringPKCE(codeVerifierRandomStringLength);
    return QByteArray::fromStdString(str).toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

QString OAuthLoginService::generateCodeChallenge(const QString &codeVerifier) const {
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(codeVerifier.toUtf8());
    return hash.result().toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

} // namespace KDC
