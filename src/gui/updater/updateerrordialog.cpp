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

#include "updateerrordialog.h"

#include "../wizard/webview.h"
#include "common/utility.h"
#include "libcommon/utility/utility.h"
#include "../guiutility.h"
#include "config.h"
#include "../parameterscache.h"
#include "libcommon/theme/theme.h"
#include "libcommon/utility/utility.h"

#include <QLabel>
#include <QNetworkReply>
#include <QPushButton>
#include <QTextEdit>

namespace KDC {

static const int mainBoxHMargin = 40;
static const int mainBoxVBMargin = 40;
static const int boxHSpacing = 10;

UpdateErrorDialog::UpdateErrorDialog(const QString &targetVersion, const QString &targetVersionString,
                                     const QString &clientVersion, QWidget *parent)
    : CustomDialog(false, parent) {
    KDC::GuiUtility::setStyle(qApp, false);
    initUi(targetVersion, targetVersionString, clientVersion);
}

UpdateErrorDialog::~UpdateErrorDialog() {
    KDC::GuiUtility::setStyle(qApp);
}

void UpdateErrorDialog::initUi(const QString &targetVersion, const QString &targetVersionString, const QString &clientVersion) {
    QVBoxLayout *mainLayout = qobject_cast<QVBoxLayout *>(layout());

    QVBoxLayout *subLayout = new QVBoxLayout();
    mainLayout->addLayout(subLayout);

    subLayout->setContentsMargins(mainBoxHMargin, 0, mainBoxHMargin, mainBoxVBMargin);
    subLayout->setSpacing(boxHSpacing);

    QLabel *lbl = new QLabel;
    QString txt = tr("<p>A new version of the %1 Client is available but the updating process failed.</p>"
                     "<p><b>%2</b> has been downloaded. The installed version is %3.</p>")
                      .arg(KDC::CommonUtility::escape(Theme::instance()->appNameGUI()),
                           KDC::CommonUtility::escape(targetVersionString), KDC::CommonUtility::escape(clientVersion));

    lbl->setText(txt);
    lbl->setObjectName("textLabel");
    lbl->setContextMenuPolicy(Qt::PreventContextMenu);
    lbl->setWordWrap(true);
    subLayout->addWidget(lbl);

    QLabel *releaseNoteLabel = new QLabel;
    releaseNoteLabel->setText(tr("Release Notes:"));
    releaseNoteLabel->setObjectName("largeMediumTextLabel");
    releaseNoteLabel->setContextMenuPolicy(Qt::PreventContextMenu);
    releaseNoteLabel->setWordWrap(true);
    subLayout->addWidget(releaseNoteLabel);

    WebView *webview = new WebView(this);
    Language language = ParametersCache::instance()->parametersInfo().language();
    QString languageCode = KDC::CommonUtility::languageCode(language);

    if (languageCode.isEmpty() || languageCode.startsWith("en")) {
        webview->setUrl(QUrl(QString("%1-%2-win.html").arg(APPLICATION_STORAGE_URL, targetVersion)));
    } else {
        webview->setUrl(QUrl(QString("%1-%2-win-%3.html").arg(APPLICATION_STORAGE_URL, targetVersion, languageCode.left(2))));
    }
    subLayout->addWidget(webview);

    QHBoxLayout *hLayout = new QHBoxLayout();

    QPushButton *skipButton = new QPushButton(tr("Skip this version"), this);
    skipButton->setObjectName("defaultsmallbutton");
    skipButton->setFlat(true);
    connect(skipButton, &QPushButton::clicked, this, &UpdateErrorDialog::skip);
    connect(skipButton, &QPushButton::clicked, this, &UpdateErrorDialog::reject);
    hLayout->addWidget(skipButton);

    hLayout->addStretch();

    QPushButton *askAgainButton = new QPushButton(tr("Remind me later"), this);
    askAgainButton->setObjectName("defaultsmallbutton");
    askAgainButton->setFlat(true);
    connect(askAgainButton, &QPushButton::clicked, this, &UpdateErrorDialog::askagain);
    connect(askAgainButton, &QPushButton::clicked, this, &UpdateErrorDialog::reject);
    hLayout->addWidget(askAgainButton);

    QPushButton *retryButton = new QPushButton(tr("Install update"), this);
    retryButton->setObjectName("defaultsmallbutton");
    retryButton->setFlat(true);
    connect(retryButton, &QPushButton::clicked, this, &UpdateErrorDialog::retry);
    connect(retryButton, &QPushButton::clicked, this, &UpdateErrorDialog::accept);
    hLayout->addWidget(retryButton);

    connect(this, &CustomDialog::exit, this, &UpdateErrorDialog::reject);

    subLayout->addLayout(hLayout);
}

}  // namespace KDC
