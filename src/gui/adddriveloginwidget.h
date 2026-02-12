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

#pragma once

#include "wizard/webview.h"
#include "libcommon/info/userinfo.h"

#include <QRadioButton>
#include <QString>
#include <QUrlQuery>
#include <QWidget>

namespace KDC {

class OAuthUrlHandler : public QObject {
        Q_OBJECT

    public:
        explicit OAuthUrlHandler(QObject *parent = nullptr) :
            QObject(parent) {}

        // À connecter au signal de QOAuth2AuthorizationCodeFlow
        void handleUrl(const QUrl &url);

    signals:
        void authorizationCodeReceived(const QString &code, const QString &state);

    protected:
        bool eventFilter(QObject *obj, QEvent *event) {
            if (event->type() == QEvent::FileOpen) {
                QFileOpenEvent *openEvent = static_cast<QFileOpenEvent *>(event);
                QUrl url = openEvent->url();

                if (url.scheme() == "kdrive" && url.host() == "auth-desktop") {
                    // Extraire code et state des query parameters
                    QUrlQuery query(url);
                    QString code = query.queryItemValue("code");
                    QString state = query.queryItemValue("state");

                    emit authorizationCodeReceived(code, state);
                    return true; // Événement consommé
                }
            }
            return QObject::eventFilter(obj, event);
        }
};


class AddDriveLoginWidget : public QWidget {
        Q_OBJECT

    public:
        explicit AddDriveLoginWidget(QWidget *parent = nullptr);

        void init();

        inline int userDbId() const { return _userDbId; }

    public slots:
        void onAuthorizationCodeReceived(const QString &code, const QString &state);

    signals:
        void terminated(bool next = true);

    private:
        QString _codeVerifier;
        int _userDbId{0};
        OAuthUrlHandler _urlHandler;

        QUrl generateAuthorizeUrl();

        QString generateCodeVerifier() const;
        QString generateCodeChallenge(const QString &codeVerifier) const;

    private slots:
        void onErrorReceived(const QString error, const QString errorDescr);
        void onOpenLoginInBrowser();
};

} // namespace KDC
