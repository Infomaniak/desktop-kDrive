/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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

#include <QUrl>
#include <QWidget>
#include <QWebEnginePage>

class QWebEngineView;
class QWebEngineProfile;

namespace KDC {

namespace Ui {
class WebView;
}

class WebViewPageUrlRequestInterceptor;
class WebViewPageUrlSchemeHandler;
class WebEnginePage;

class WebView : public QWidget {
        Q_OBJECT
    public:
        WebView(QWidget *parent = nullptr);
        virtual ~WebView();
        void setUrl(const QUrl &url);

    signals:
        void urlCatched(const QString user, const QString pass, const QString host);
        void authorizationCodeUrlCatched(const QString code, const QString state);
        void errorCatched(const QString error, const QString errorDescr);

    protected slots:
        void loadStarted();
        void loadFinished(bool ok);
        void renderProcessTerminated(QWebEnginePage::RenderProcessTerminationStatus terminationStatus, int exitCode);

    private:
        Ui::WebView *_ui;

        QWebEngineView *_webview;
        QWebEngineProfile *_profile;
        WebEnginePage *_page;

        WebViewPageUrlSchemeHandler *_schemeHandler;
};

}  // namespace KDC
