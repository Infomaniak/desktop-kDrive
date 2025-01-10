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

#include "updatedialog.h"

#include "../wizard/webview.h"
#include "libcommon/utility/utility.h"
#include "../guiutility.h"
#include "config.h"
#include "../parameterscache.h"
#include "libcommon/theme/theme.h"

#include <QLabel>
#include <QNetworkReply>
#include <QPushButton>
#include <QTextEdit>

namespace KDC {

static const int mainBoxHMargin = 40;
static const int mainBoxVBMargin = 40;
static const int boxHSpacing = 10;

UpdateDialog::UpdateDialog(const VersionInfo &versionInfo, QWidget *parent /*= nullptr*/) : CustomDialog(false, parent) {
    KDC::GuiUtility::setStyle(qApp, false);
    initUi(versionInfo);
}

UpdateDialog::~UpdateDialog() {
    KDC::GuiUtility::setStyle(qApp);
}

void UpdateDialog::initUi(const VersionInfo &versionInfo) {
    auto *mainLayout = qobject_cast<QVBoxLayout *>(layout());

    auto *subLayout = new QVBoxLayout();
    mainLayout->addLayout(subLayout);

    subLayout->setContentsMargins(mainBoxHMargin, 0, mainBoxHMargin, mainBoxVBMargin);
    subLayout->setSpacing(boxHSpacing);

    auto *lbl = new QLabel;
    QString txt = tr("<p>The new version <b>%1</b> of the %2 Client is available and has been downloaded.</p>"
                     "<p>The installed version is %3.</p>")
                          .arg(KDC::CommonUtility::escape(versionInfo.beautifulVersion().c_str()),
                               KDC::CommonUtility::escape(Theme::instance()->appNameGUI()),
                               KDC::CommonUtility::escape(CommonUtility::currentVersion().c_str()));

    lbl->setText(txt);
    lbl->setObjectName("textLabel");
    lbl->setContextMenuPolicy(Qt::PreventContextMenu);
    lbl->setWordWrap(true);
    subLayout->addWidget(lbl);

    auto *releaseNoteLabel = new QLabel;
    releaseNoteLabel->setText(tr("Release Notes:"));
    releaseNoteLabel->setObjectName("largeMediumTextLabel");
    releaseNoteLabel->setContextMenuPolicy(Qt::PreventContextMenu);
    releaseNoteLabel->setWordWrap(true);
    subLayout->addWidget(releaseNoteLabel);

    auto *webview = new WebView(this);
    const Language language = ParametersCache::instance()->parametersInfo().language();
    QString languageCode = CommonUtility::languageCode(language);
    if (languageCode.isEmpty()) languageCode = "en";
    webview->setUrl(QUrl(
            QString("%1-%2-win-%3.html").arg(APPLICATION_STORAGE_URL, versionInfo.fullVersion().c_str(), languageCode.left(2))));
    subLayout->addWidget(webview);

    auto *hLayout = new QHBoxLayout();

    _skipButton = new QPushButton(tr("Skip this version"), this);
    _skipButton->setObjectName("defaultsmallbutton");
    _skipButton->setFlat(true);
    connect(_skipButton, &QPushButton::clicked, this, &UpdateDialog::reject);
    hLayout->addWidget(_skipButton);

    hLayout->addStretch();

    auto *askAgainButton = new QPushButton(tr("Remind me later"), this);
    askAgainButton->setObjectName("defaultsmallbutton");
    askAgainButton->setFlat(true);
    connect(askAgainButton, &QPushButton::clicked, this, &UpdateDialog::reject);
    hLayout->addWidget(askAgainButton);

    auto *installButton = new QPushButton(tr("Install update"), this);
    installButton->setObjectName("defaultsmallbutton");
    installButton->setFlat(true);
    connect(installButton, &QPushButton::clicked, this, &UpdateDialog::accept);
    hLayout->addWidget(installButton);

    connect(this, &CustomDialog::exit, this, &UpdateDialog::reject);

    subLayout->addLayout(hLayout);
}

void UpdateDialog::reject() {
    if (sender() == _skipButton) _skip = true;
    QDialog::reject();
}

} // namespace KDC
