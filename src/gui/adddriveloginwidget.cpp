/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
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

#include "adddriveloginwidget.h"
#include "libcommongui/commclient.h"
#include "guiutility.h"
#include "config.h"
#include "guirequests.h"
#include "custommessagebox.h"
#include "libcommon/theme/theme.h"
#include "libcommon/utility/utility.h"
#include "libcommongui/utility/utility.h"
#include "utility/urlhelper.h"

#include <QBoxLayout>
#include <QLabel>
#include <QLoggingCategory>
#include <QProcessEnvironment>
#include <QUrlQuery>

namespace KDC {

const QString responseTypeKey = "response_type";
const QString clientIdKey = "client_id";
const QString redirectUriKey = "redirect_uri";
const QString codeChallengeMethodKey = "code_challenge_method";
const QString codeChallengeKey = "code_challenge";
const QString stateKey = "state";

const QString hashMode = "S256"; // SHA-256
const QString responseType = "code";
const QString authorizePath = "/authorize";

const int stateStringLength = 8;

Q_LOGGING_CATEGORY(lcAddDriveLoginWidget, "gui.adddriveloginwidget", QtInfoMsg)

AddDriveLoginWidget::AddDriveLoginWidget(QWidget *parent) :
    QWidget(parent),
    _codeVerifier(QString()),
    _userDbId(0),
    _webView(nullptr) {
    QVBoxLayout *mainLayout = new QVBoxLayout();
    mainLayout->setContentsMargins(0, 0, 0, 0);
    setLayout(mainLayout);

    _webView = new WebView(this);
    mainLayout->addWidget(_webView);

    connect(_webView, &WebView::authorizationCodeUrlCatched, this, &AddDriveLoginWidget::onAuthorizationCodeReceived);
    connect(_webView, &WebView::errorCatched, this, &AddDriveLoginWidget::onErrorReceived);
}

void AddDriveLoginWidget::init(const QString &serverUrl) {
    QString loginUrl(serverUrl);
    if (!loginUrl.endsWith(dirSeparator)) {
        loginUrl += dirSeparator;
    }

    _webView->setUrl(QUrl(loginUrl));
}

void AddDriveLoginWidget::init() {
    refreshPage();
    _webView->setUrl(generateUrl());
}

void AddDriveLoginWidget::onAuthorizationCodeReceived(const QString code, const QString state) {
    Q_UNUSED(state)

    QString error;
    QString errorDescr;
    ExitCode exitCode = GuiRequests::requestToken(code, _codeVerifier, _userDbId, error, errorDescr);
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcAddDriveLoginWidget()) << "Error in Requests::requestToken: code=" << exitCode;

        CustomMessageBox msgBox(QMessageBox::Warning, tr("Token request failed: %1 - %2").arg(error, errorDescr),
                                QMessageBox::Ok);
        msgBox.exec();

        emit terminated(false);
    } else {
        emit terminated();
    }
}

void AddDriveLoginWidget::onErrorReceived(const QString error, const QString errorDescr) {
    qCWarning(lcAddDriveLoginWidget) << "Login failed : " << error.toStdString().c_str() << " - "
                                     << errorDescr.toStdString().c_str();

    CustomMessageBox msgBox(QMessageBox::Warning, tr("Login failed: %1 - %2").arg(error, errorDescr), QMessageBox::Ok);
    msgBox.exec();

    emit terminated(false);
}

QUrl AddDriveLoginWidget::generateUrl() {
    QUrl url(UrlHelper::loginApiUrl().c_str());
    url.setPath(authorizePath);

    _codeVerifier = generateCodeVerifier();
    const QString codeChallenge = generateCodeChallenge(_codeVerifier);

    QUrlQuery query;
    query.addQueryItem(responseTypeKey, responseType);
    query.addQueryItem(clientIdKey, CLIENT_ID);
    query.addQueryItem(redirectUriKey, REDIRECT_URI);
    query.addQueryItem(codeChallengeMethodKey,
                       hashMode); // TODO : server return an error : invalid_request - invalid+challenge_code+format
    query.addQueryItem(codeChallengeKey, codeChallenge);
    query.addQueryItem(stateKey, CommonUtility::generateRandomStringAlphaNum(stateStringLength).c_str());

    url.setQuery(query);

    return url;
}

const QString AddDriveLoginWidget::generateCodeVerifier() {
    std::string str = CommonUtility::generateRandomStringPKCE(32);
    return QByteArray::fromStdString(str).toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

const QString AddDriveLoginWidget::generateCodeChallenge(const QString &codeVerifier) {
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(codeVerifier.toUtf8());
    return hash.result().toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

void AddDriveLoginWidget::refreshPage() {
    delete _webView;
    _webView = new WebView(this);
    layout()->addWidget(_webView);
    connect(_webView, &WebView::authorizationCodeUrlCatched, this, &AddDriveLoginWidget::onAuthorizationCodeReceived);
    connect(_webView, &WebView::errorCatched, this, &AddDriveLoginWidget::onErrorReceived);
}

} // namespace KDC
