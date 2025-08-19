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

#include "webview.h"

#include "../parameterscache.h"
#include "ui_webview.h"
#include "libcommongui/matomoclient.h"
#include "libcommon/utility/utility.h"

#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineUrlRequestJob>
#include <QWebEngineUrlScheme>
#include <QWebEngineUrlSchemeHandler>
#include <QWebEngineView>
#include <QWebEngineSettings>
#include <QDesktopServices>
#include <QProgressBar>
#include <QLoggingCategory>
#include <QLocale>
#include <QWebEngineCertificateError>
#include <QWebEngineCookieStore>
#include <QMessageBox>

#include "config.h"

namespace KDC {

Q_LOGGING_CATEGORY(lcWizardWebiew, "gui.wizard.webview", QtInfoMsg)

class WebViewPageUrlSchemeHandler : public QWebEngineUrlSchemeHandler {
        Q_OBJECT

    public:
        WebViewPageUrlSchemeHandler(QObject *parent = nullptr);
        void requestStarted(QWebEngineUrlRequestJob *request) override;

    Q_SIGNALS:
        void urlCatched(QString user, QString pass, QString host);
        void authorizationCodeUrlCatched(QString code, QString state);
        void errorCatched(QString error, QString errorDescription);
};

class WebEnginePage : public QWebEnginePage {
        Q_OBJECT

    public:
        WebEnginePage(QWebEngineProfile *profile, QObject *parent = nullptr);
        void setUrl(const QUrl &url);

    private:
        QUrl _rootUrl;
};

WebView::WebView(QWidget *parent) :
    QWidget(parent),
    _ui(new Ui::WebView) {
    _ui->setupUi(this);

    QWebEngineUrlScheme _ncsheme("kdrive");
    QWebEngineUrlScheme::registerScheme(_ncsheme);

    _webview = new QWebEngineView(this);
    _profile = new QWebEngineProfile(this);
    _page = new WebEnginePage(_profile);
    _schemeHandler = new WebViewPageUrlSchemeHandler(this);

#if defined(Q_OS_LINUX) && defined(Q_PROCESSOR_ARM)
    // On ARM Linux with QT 6.7.3, neither Chromium nor Qt automatically trigger a webview update.
    // Only certain user actions can refresh the page.
    // Therefore, we use a timer to force the refresh.
    auto *redrawTimer = new QTimer(this);
    connect(redrawTimer, &QTimer::timeout, _webview, QOverload<>::of(&QWidget::update));
    redrawTimer->start(16); // Approximately 60 fps (1000/16=62.5)
#endif

    const QString userAgent(CommonUtility::userAgentString().c_str());
    _profile->setHttpUserAgent(userAgent);
    QWebEngineProfile::defaultProfile()->setHttpUserAgent(userAgent);
    _profile->installUrlSchemeHandler("kdrive", _schemeHandler);

    /*
     * Set a proper accept language to the language of the client
     * code from: http://code.qt.io/cgit/qt/qtbase.git/tree/src/network/access/qhttpnetworkconnection.cpp
     */
    {
        const auto language = ParametersCache::instance()->parametersInfo().language();
        const auto languageCode = CommonUtility::languageCode(language);
        QString acceptLanguage;
        if (languageCode == QLatin1String("C") || CommonUtility::languageCodeIsEnglish(languageCode)) {
            acceptLanguage = QString::fromLatin1("en,*");
        } else {
            acceptLanguage = languageCode + QLatin1String(",en,*");
        }
        _profile->setHttpAcceptLanguage(acceptLanguage);
    }

    _page->setBackgroundColor(Qt::transparent);
    _webview->setPage(_page);
    _webview->settings()->setAttribute(QWebEngineSettings::ShowScrollBars, false);
    _ui->verticalLayout->addWidget(_webview);

    static const auto webviewDebugPort = CommonUtility::envVarValue("QTWEBENGINE_REMOTE_DEBUGGING");
    if (!webviewDebugPort.empty()) {
        _inspectorView = new QWebEngineView(this);
        _webview->page()->setDevToolsPage(_inspectorView->page());
    }

    connect(_webview, &QWebEngineView::loadProgress, _ui->progressBar, &QProgressBar::setValue);
    connect(_webview, &QWebEngineView::loadStarted, this, &WebView::loadStarted);
    connect(_webview, &QWebEngineView::loadFinished, this, &WebView::loadFinished);
    connect(_webview, &QWebEngineView::renderProcessTerminated, this, &WebView::renderProcessTerminated);
    connect(_schemeHandler, &WebViewPageUrlSchemeHandler::urlCatched, this, &WebView::urlCatched);
    connect(_schemeHandler, &WebViewPageUrlSchemeHandler::authorizationCodeUrlCatched, this,
            &WebView::authorizationCodeUrlCatched);
    connect(_schemeHandler, &WebViewPageUrlSchemeHandler::errorCatched, this, &WebView::errorCatched);
}

void WebView::setUrl(const QUrl &url) {
    _page->setUrl(url);
}

void WebView::loadStarted() {
    _ui->progressBar->setVisible(true);
}

void WebView::loadFinished(bool ok) {
    qCInfo(lcWizardWebiew()) << ok;
    _ui->progressBar->setVisible(false);

    if (ok) { // Send Matomo visitPage
        const QString host = _webview->url().host();

        if (host.contains("login.infomaniak.com", Qt::CaseSensitive)) { // Login Webview
            MatomoClient::sendVisit(MatomoNameField::WV_LoginPage);
        } else { // Other Webview, shouldn't happen, there is no other Qt webview in the codebase.
            MatomoClient::sendVisit(MatomoNameField::Unknown);
        }
    }
}

void WebView::renderProcessTerminated(QWebEnginePage::RenderProcessTerminationStatus terminationStatus, int exitCode) {
    qCInfo(lcWizardWebiew()) << terminationStatus << " code=" << exitCode;
    // Trick to force size update
    const auto height = contentsRect().height();
    (void) height;
}

WebView::~WebView() {
    delete _page;
    delete _profile;
    delete _webview;
    delete _schemeHandler;
}

WebViewPageUrlSchemeHandler::WebViewPageUrlSchemeHandler(QObject *parent) :
    QWebEngineUrlSchemeHandler(parent) {}

void WebViewPageUrlSchemeHandler::requestStarted(QWebEngineUrlRequestJob *request) {
    QUrl url = request->requestUrl();

    QString query = url.query();
    QStringList parts = query.split("&");

    QString code, state, error, errorDescr;
    bool ok = true;
    for (const QString &part: parts) {
        if (part.startsWith("code")) {
            code = part.split("=").last();
        } else if (part.startsWith("state")) {
            state = part.split("=").last();
        } else if (part.startsWith("error_description")) {
            errorDescr = part.split("=").last();
            ok = false;
        } else if (part.startsWith("error")) {
            error = part.split("=").last();
            ok = false;
        }
    }

    if (ok) {
        emit authorizationCodeUrlCatched(code, state);
    } else {
        emit errorCatched(error, errorDescr);
    }
}


WebEnginePage::WebEnginePage(QWebEngineProfile *profile, QObject *parent) :
    QWebEnginePage(profile, parent) {}

void WebEnginePage::setUrl(const QUrl &url) {
    qCInfo(lcWizardWebiew()) << url;
    QWebEnginePage::setUrl(url);
    _rootUrl = url;
}

} // namespace KDC

#include "webview.moc"
