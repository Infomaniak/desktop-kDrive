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

#include "aboutdialog.h"
#include "custommessagebox.h"
#include "guiutility.h"
#include "common/utility.h"
#include "libcommon/utility/utility.h"
#include "version.h"
#include "config.h"
#include "libcommon/theme/theme.h"

#include <QBoxLayout>
#include <QDesktopServices>
#include <QLabel>
#include <QPushButton>
#include <QSslSocket>
#include <QLoggingCategory>

namespace KDC {

static const int boxHMargin = 40;
static const int boxHSpacing = 10;
static const int titleBoxVMargin = 14;
static const int logoBoxVMargin = 10;
static const int hLogoSpacing = 10;
static const int logoIconSize = 39;
static const QSize logoTextIconSize = QSize(60, 42);

static const QString domainLink = "domainLink";
static const QString gitLink = "gitLink";
static const QString gnuLink = "gnuLink";

static const QString githubPrefix = "https://github.com/infomaniak/desktop-kDrive";
static const QUrl gnuUrl("https://www.gnu.org/licenses/lgpl-3.0.fr.html#license-text");

Q_LOGGING_CATEGORY(lcAboutDialog, "gui.aboutdialog", QtInfoMsg)

AboutDialog::AboutDialog(QWidget *parent) : CustomDialog(true, parent), _logoColor(QColor()), _logoTextIconLabel(nullptr) {
    initUI();
}

void AboutDialog::setLogoColor(const QColor &color) {
    _logoColor = color;
    _logoTextIconLabel->setPixmap(KDC::GuiUtility::getIconWithColor(":/client/resources/logos/kdrive-text-only.svg", _logoColor)
                                          .pixmap(logoTextIconSize));
}

void AboutDialog::initUI() {
    QVBoxLayout *mainLayout = this->mainLayout();

    // Title
    QLabel *titleLabel = new QLabel(this);
    titleLabel->setObjectName("titleLabel");
    titleLabel->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    titleLabel->setText(tr("About"));
    mainLayout->addWidget(titleLabel);
    mainLayout->addSpacing(titleBoxVMargin);

    // Logo
    QHBoxLayout *logoHBox = new QHBoxLayout();
    logoHBox->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    mainLayout->addLayout(logoHBox);
    mainLayout->addSpacing(logoBoxVMargin);

    QLabel *logoIconLabel = new QLabel(this);
    logoIconLabel->setPixmap(KDC::GuiUtility::getIconWithColor(":/client/resources/logos/kdrive-without-text.svg")
                                     .pixmap(QSize(logoIconSize, logoIconSize)));
    logoHBox->addWidget(logoIconLabel);
    logoHBox->addSpacing(hLogoSpacing);

    _logoTextIconLabel = new QLabel(this);
    logoHBox->addWidget(_logoTextIconLabel);
    logoHBox->addStretch();

    // Text
    QLabel *textLabel = new QLabel(this);
    textLabel->setObjectName("textLabel");
    textLabel->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    textLabel->setWordWrap(true);
    textLabel->setText(aboutText());
    textLabel->setContextMenuPolicy(Qt::PreventContextMenu);
    mainLayout->addWidget(textLabel);
    mainLayout->addStretch();

    // Add dialog buttons
    QHBoxLayout *buttonsHBox = new QHBoxLayout();
    buttonsHBox->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    buttonsHBox->setSpacing(boxHSpacing);
    mainLayout->addLayout(buttonsHBox);

    QPushButton *closeButton = new QPushButton(this);
    closeButton->setObjectName("defaultbutton");
    closeButton->setFlat(true);
    closeButton->setText(tr("CLOSE"));
    buttonsHBox->addWidget(closeButton);
    buttonsHBox->addStretch();

    connect(closeButton, &QPushButton::clicked, this, &AboutDialog::onExit);
    connect(this, &CustomDialog::exit, this, &AboutDialog::onExit);
    connect(textLabel, &QLabel::linkActivated, this, &AboutDialog::onLinkActivated);
}

QString AboutDialog::aboutText() const {
    QString about;
    about = tr("Version %1. For more information visit <a style=\"%2\" href=\"%3\">%4</a><br><br>")
                    .arg(KDC::CommonUtility::escape(KDRIVE_VERSION_STRING), CommonUtility::linkStyle, domainLink,
                         "https://" KDRIVE_STRINGIFY(APPLICATION_DOMAIN));
    about += tr("Copyright 2019-%1 Infomaniak Network SA<br><br>").arg(QDate::currentDate().year());
    about += tr("Distributed by %1 and licensed under the <a style=\"%3\" href=\"%4\">%5</a>.<br><br>"
                "%2 and the %2 logo are registered trademarks of %1.<br><br>")
                     .arg(KDC::CommonUtility::escape(APPLICATION_VENDOR), KDC::CommonUtility::escape(APPLICATION_NAME),
                          CommonUtility::linkStyle, gnuLink, "GNU Lesser General Public License (LGPL) Version 3.0");

    about += tr("<p><small>Built from <a style=\"color: #489EF3\" href=\"%1\">Git sources</a> on %2, %3 using Qt %4, "
                "%5</small></p>")
                     .arg(gitLink, __DATE__, __TIME__, qVersion(), QSslSocket::sslLibraryVersionString());

    return about;
}

void AboutDialog::onExit() {
    accept();
}

void AboutDialog::onLinkActivated(const QString &link) {
    if (link == domainLink) {
        QUrl domainUrl = QUrl("https://" KDRIVE_STRINGIFY(APPLICATION_DOMAIN));
        if (domainUrl.isValid()) {
            if (!QDesktopServices::openUrl(domainUrl)) {
                qCWarning(lcAboutDialog) << "QDesktopServices::openUrl failed for " << domainUrl.toString();
                CustomMessageBox msgBox(QMessageBox::Warning, tr("Unable to open folder %1.").arg(domainUrl.toString()),
                                        QMessageBox::Ok, this);
                msgBox.exec();
            }
        }
    } else if (link == gitLink) {
        QUrl gitUrl = QUrl(githubPrefix);
        if (gitUrl.isValid()) {
            if (!QDesktopServices::openUrl(gitUrl)) {
                qCWarning(lcAboutDialog) << "QDesktopServices::openUrl failed for " << gitUrl.toString();
                CustomMessageBox msgBox(QMessageBox::Warning, tr("Unable to open folder %1.").arg(gitUrl.toString()),
                                        QMessageBox::Ok, this);
                msgBox.exec();
            }
        }
    } else if (link == gnuLink) {
        if (!QDesktopServices::openUrl(gnuUrl)) {
            qCWarning(lcAboutDialog) << "QDesktopServices::openUrl failed for " << gnuUrl.toString();
            CustomMessageBox msgBox(QMessageBox::Warning, tr("Unable to open folder %1.").arg(gnuUrl.toString()), QMessageBox::Ok,
                                    this);
            msgBox.exec();
        }
    }
    accept();
}

} // namespace KDC
