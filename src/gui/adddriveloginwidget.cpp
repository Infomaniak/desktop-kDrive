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

#include "adddriveloginwidget.h"
#include "libcommongui/commclient.h"
#include "guiutility.h"
#include "config.h"
#include "guirequests.h"
#include "custommessagebox.h"
#include "custompushbutton.h"
#include "matomoclient.h"
#include "libcommon/theme/theme.h"
#include "libcommon/utility/utility.h"
#include "libcommongui/utility/utility.h"
#include "log/sentry/handler.h"
#include "utility/urlhelper.h"

#include <QBoxLayout>
#include <QLabel>
#include <QDesktopServices>
#include <QUrlQuery>

namespace KDC {

const QString responseTypeKey = "response_type";
const QString clientIdKey = "client_id";
const QString redirectUriKey = "redirect_uri";
const QString codeChallengeMethodKey = "code_challenge_method";
const QString codeChallengeKey = "code_challenge";
const QString stateKey = "state";
const QString skipAutoRedirectKey = "skipAutoRedirect";

const QString hashMode = "S256"; // SHA-256
const QString responseType = "code";
const QString authorizePath = "/authorize";

const int stateStringLength = 8;

Q_LOGGING_CATEGORY(lcAddDriveLoginWidget, "gui.adddriveloginwidget", QtInfoMsg)


AddDriveLoginWidget::AddDriveLoginWidget(QWidget *parent) :
    QWidget(parent) {
    auto *const mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    setLayout(mainLayout);
    mainLayout->addStretch();

    // Left part
    auto *leftPartLayout = new QVBoxLayout(this);
    mainLayout->addItem(leftPartLayout);
    leftPartLayout->addStretch(10);

    auto *titleLabel = new QLabel(tr("Log in from your browser"), this);
    titleLabel->setObjectName("titleLabel");
    leftPartLayout->addWidget(titleLabel);

    leftPartLayout->addStretch(1);

    auto *subTitleLabel = new QLabel(tr("Your browser should open automatically to complete the connection. Once connected, you "
                                        "will automatically return to kDrive."),
                                     this);
    subTitleLabel->setObjectName("textLabel");
    subTitleLabel->setWordWrap(true);
    subTitleLabel->setMinimumWidth(400);
    leftPartLayout->addWidget(subTitleLabel);

    leftPartLayout->addStretch(1);

    auto *connectButton = new CustomPushButton(tr("Open the login page"), this);
    connectButton->setObjectName("nondefaultbutton");
    connectButton->setFlat(true);
    connectButton->setMaximumWidth(250);
    leftPartLayout->addWidget(connectButton);
    (void) connect(connectButton, &CustomPushButton::clicked, this, &AddDriveLoginWidget::onOpenLoginInBrowser);

    leftPartLayout->addStretch(10);

    mainLayout->addStretch();
    mainLayout->addStretch();

    // Right part
    auto *logoIconLabel = new QLabel(this);
    logoIconLabel->setPixmap(
            KDC::GuiUtility::getIconWithColor(":/client/resources/logos/kdrive-loader-stroke.svg").pixmap(150, 130));
    mainLayout->addWidget(logoIconLabel);

    mainLayout->addStretch();
}

void AddDriveLoginWidget::init() {
    onOpenLoginInBrowser();
}

void AddDriveLoginWidget::onAuthorizationCodeReceived(const QString &code, const QString &state) {
    Q_UNUSED(state)

    QString error;
    QString errorDescr;
    const auto exitCode = GuiRequests::requestToken(code, _codeVerifier, _userDbId, error, errorDescr);
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcAddDriveLoginWidget()) << "Error in Requests::requestToken: code=" << exitCode;

        CustomMessageBox msgBox(QMessageBox::Warning, tr("Token request failed: %1 - %2").arg(error, errorDescr),
                                QMessageBox::Ok);
        (void) msgBox.exec();

        emit terminated(false);
    } else {
        emit terminated();
    }
}

void AddDriveLoginWidget::onErrorReceived(const QString error, const QString errorDescr) {
    qCWarning(lcAddDriveLoginWidget) << "Login failed : " << error.toStdString().c_str() << " - "
                                     << errorDescr.toStdString().c_str();

    CustomMessageBox msgBox(QMessageBox::Warning, tr("Login failed: %1 - %2").arg(error, errorDescr), QMessageBox::Ok);
    (void) msgBox.exec();

    emit terminated(false);
}

void AddDriveLoginWidget::onOpenLoginInBrowser() {
    if (!QDesktopServices::openUrl(generateAuthorizeUrl())) {
        const auto errorMsg = tr("Failed to open the login page in your web browser");
        sentry::Handler::captureMessage(sentry::Level::Warning, "Login failed", errorMsg.toStdString());
        CustomMessageBox msgBox(QMessageBox::Warning, errorMsg, QMessageBox::Ok);
        (void) msgBox.exec();


        emit terminated(false);
        return;
    }
    MatomoClient::sendVisit(MatomoNameField::WV_LoginPage);
}

QUrl AddDriveLoginWidget::generateAuthorizeUrl() {
    QUrl url(UrlHelper::loginApiUrl().c_str());
    url.setPath(authorizePath);

    _codeVerifier = generateCodeVerifier();
    const QString codeChallenge = generateCodeChallenge(_codeVerifier);

    QUrlQuery query;
    query.addQueryItem(responseTypeKey, responseType);
    query.addQueryItem(clientIdKey, CLIENT_ID);
    query.addQueryItem(redirectUriKey, REDIRECT_URI);
    query.addQueryItem(codeChallengeMethodKey, hashMode);
    query.addQueryItem(codeChallengeKey, codeChallenge);
    query.addQueryItem(stateKey, CommonUtility::generateRandomStringAlphaNum(stateStringLength).c_str());
    query.addQueryItem(skipAutoRedirectKey, "true");

    url.setQuery(query);

    return url;
}

QString AddDriveLoginWidget::generateCodeVerifier() const {
    const std::string str = CommonUtility::generateRandomStringPKCE(32);
    return QByteArray::fromStdString(str).toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

QString AddDriveLoginWidget::generateCodeChallenge(const QString &codeVerifier) const {
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(codeVerifier.toUtf8());
    return hash.result().toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

} // namespace KDC
