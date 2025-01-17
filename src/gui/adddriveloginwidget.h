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
#include <QWidget>

namespace KDC {

class AddDriveLoginWidget : public QWidget {
        Q_OBJECT

    public:
        explicit AddDriveLoginWidget(QWidget *parent = nullptr);

        void init(const QString &serverUrl);
        void init();

        inline int userDbId() const { return _userDbId; }

    signals:
        void terminated(bool next = true);

    private:
        QString _codeVerifier;
        int _userDbId;

        WebView *_webView;

        void refreshPage();

        QUrl generateUrl();

        const QString generateCodeVerifier();
        const QString generateCodeChallenge(const QString &codeVerifier);

    private slots:
        void onAuthorizationCodeReceived(const QString code, const QString state);
        void onErrorReceived(const QString error, const QString errorDescr);
};

} // namespace KDC
