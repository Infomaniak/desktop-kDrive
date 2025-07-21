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

#include "versionwidget.h"
#include "aboutdialog.h"
#include "enablestateholder.h"
#include "guirequests.h"
#include "guiutility.h"
#include "betaprogramdialog.h"
#include "parameterscache.h"
#include "preferencesblocwidget.h"
#include "taglabel.h"
#include <config.h>
#include "libcommon/utility/utility.h"
#include "libcommongui/matomoclient.h"

#include <QDesktopServices>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>
#include <version.h>

namespace KDC {

static const QString versionLink = "versionLink";
static const QString releaseNoteLink = "releaseNoteLink";
static const QString downloadPageLink = "downloadPageLink";

static constexpr int statusLayoutSpacing = 8;
static constexpr auto betaTagColor = QColor(214, 56, 100);
static constexpr auto internalTagColor = QColor(120, 116, 176);

Q_LOGGING_CATEGORY(lcVersionWidget, "gui.versionwidget", QtInfoMsg)

VersionWidget::VersionWidget(QWidget *parent /*= nullptr*/) :
    QWidget(parent) {
    const auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    setLayout(mainLayout);

    _versionLabel = new QLabel(this);
    _versionLabel->setObjectName("blocLabel");
    mainLayout->addWidget(_versionLabel);

    const auto prefBloc = new PreferencesBlocWidget();
    mainLayout->addWidget(prefBloc);

    initVersionInfoBloc(prefBloc);
    prefBloc->addSeparator();
    initBetaBloc(prefBloc);

    refresh();

    connect(_updateStatusLabel, &QLabel::linkActivated, this, &VersionWidget::onLinkActivated);
    connect(_versionNumberLabel, &QLabel::linkActivated, this, &VersionWidget::onLinkActivated);
    connect(_showReleaseNotesLabel, &QLabel::linkActivated, this, &VersionWidget::onLinkActivated);
    connect(_updateButton, &QPushButton::clicked, this, &VersionWidget::onUpdateButtonClicked);
    connect(_joinBetaButton, &QPushButton::clicked, this, &VersionWidget::onJoinBetaButtonClicked);
}

void VersionWidget::refresh(const bool isStaff) {
    _isStaff = isStaff;
    refresh();
}

void VersionWidget::showAboutDialog() {
    EnableStateHolder _(this);
    AboutDialog dialog(this);
    dialog.execAndMoveToCenter(GuiUtility::getTopLevelWidget(this));
    MatomoClient::sendVisit(MatomoNameField::PG_Preferences_About);
}

void VersionWidget::showReleaseNotes() const {
    QString os;
#if defined(KD_MACOS)
    os = "macos";
#elif defined(KD_WINDOWS)
    os = "win";
#else
    os = "linux";
#endif

    VersionInfo versionInfo;
    GuiRequests::versionInfo(versionInfo);

    const Language &appLanguage = ParametersCache::instance()->parametersInfo().language();
    QString languageCode = CommonUtility::languageCode(appLanguage);
    if (languageCode.isEmpty()) languageCode = CommonUtility::englishCode;
    QDesktopServices::openUrl(
            QUrl(QString("%1-%2-%3-%4.html")
                         .arg(APPLICATION_STORAGE_URL, versionInfo.fullVersion().c_str(), os, languageCode.left(2))));
}

void VersionWidget::showDownloadPage() const {
    QDesktopServices::openUrl(QUrl(APPLICATION_DOWNLOAD_URL));
}

void VersionWidget::onUpdateStateChanged(const UpdateState state) const {
    refresh(state);
}

void VersionWidget::onLinkActivated(const QString &link) {
    if (link == versionLink) {
        showAboutDialog();
        MatomoClient::sendEvent("versionWidget", MatomoEventAction::Click, "versionLink");
    } else if (link == releaseNoteLink) {
        showReleaseNotes();
        MatomoClient::sendEvent("versionWidget", MatomoEventAction::Click, "releaseNotesLink");
    } else if (link == downloadPageLink) {
        showDownloadPage();
        MatomoClient::sendEvent("versionWidget", MatomoEventAction::Click, "downloadPageLink");
    } else {
        qCWarning(lcVersionWidget) << "Unknown link clicked: " << link;
        Q_ASSERT(false);
    }
}

void VersionWidget::onUpdateButtonClicked() {
#if defined(KD_MACOS)
    GuiRequests::startInstaller();
#else
    VersionInfo versionInfo;
    GuiRequests::versionInfo(versionInfo);
    emit showUpdateDialog(versionInfo);
#endif
}

void VersionWidget::onJoinBetaButtonClicked() {
    MatomoClient::sendEvent("versionWidget", MatomoEventAction::Click, "joinBetaButton");
    MatomoClient::sendVisit(MatomoNameField::PG_Preferences_Beta);
    if (auto dialog = BetaProgramDialog(
                ParametersCache::instance()->parametersInfo().distributionChannel() != VersionChannel::Prod, _isStaff, this);
        dialog.exec() == QDialog::Accepted) {
        saveDistributionChannel(dialog.selectedDistributionChannel());
        refresh();
    }
}

void VersionWidget::refresh(UpdateState state /*= UpdateState::Unknown*/) const {
    // Re-translate
    const QString releaseNoteLinkText =
            tr(R"(<a style="%1" href="%2">Show release note</a>)").arg(CommonUtility::linkStyle, releaseNoteLink);
    _showReleaseNotesLabel->setText(releaseNoteLinkText);

    _versionLabel->setText(tr("Version"));
    _updateButton->setText(tr("UPDATE"));

    // Refresh update state
    if (state == UpdateState::Unknown) {
        GuiRequests::updateState(state);
    }
    VersionInfo versionInfo;
    GuiRequests::versionInfo(versionInfo);
    const QString versionStr = versionInfo.beautifulVersion().c_str();

    QString statusString;
    bool showReleaseNote = false;
    bool showUpdateButton = false;
    switch (state) {
        case UpdateState::UpToDate: {
            statusString = tr("%1 is up to date!").arg(APPLICATION_NAME);
            break;
        }
        case UpdateState::Checking: {
            statusString = tr("Checking update on server...");
            break;
        }
        case UpdateState::ManualUpdateAvailable: {
            statusString = tr(R"(An update is available: %1.<br>Please download it from <a style="%2" href="%3">here</a>.)")
                                   .arg(versionStr, CommonUtility::linkStyle, downloadPageLink);
            showReleaseNote = true;
            break;
        }
        case UpdateState::Available:
        case UpdateState::Ready: {
            statusString = tr("An update is available: %1").arg(versionStr);
            showReleaseNote = true;
            showUpdateButton = true;
            break;
        }
        case UpdateState::Downloading: {
            statusString = tr("Downloading %1. Please wait...").arg(versionStr);
            showReleaseNote = true;
            break;
        }
        case UpdateState::CheckError: {
            statusString = tr("Could not check for new updates.");
            break;
        }
        case UpdateState::UpdateError: {
            statusString = tr("An error occurred during update.");
            break;
        }
        case UpdateState::DownloadError: {
            statusString = tr("Could not download update.");
            break;
        }
        case UpdateState::Unknown:
            break;
        case UpdateState::EnumEnd: {
            assert(false && "Invalid enum value in switch statement.");
        }
    }

    _updateStatusLabel->setText(statusString);
    _showReleaseNotesLabel->setVisible(showReleaseNote);
    _updateButton->setVisible(showUpdateButton);

    // Beta version info
    if (_betaVersionLabel) {
        _betaVersionLabel->setText(tr("Beta program"));
        _betaVersionDescription->setText(tr("Get early access to new versions of the application"));

        if (const auto channel = ParametersCache::instance()->parametersInfo().distributionChannel();
            channel == VersionChannel::Prod) {
            _joinBetaButton->setText(tr("Join"));
            _betaTag->setVisible(false);
        } else {
            _joinBetaButton->setText(_isStaff ? tr("Modify") : tr("Quit"));
            _betaTag->setVisible(true);
            _betaTag->setBackgroundColor(channel == VersionChannel::Beta ? betaTagColor : internalTagColor);
            _betaTag->setText(channel == VersionChannel::Beta ? "BETA" : "INTERNAL");
        }
    }
}

void VersionWidget::initVersionInfoBloc(PreferencesBlocWidget *prefBloc) {
    auto *versionLayout = prefBloc->addLayout(QBoxLayout::Direction::LeftToRight);

    auto *verticalLayout = new QVBoxLayout(this);
    verticalLayout->setSpacing(1);
    verticalLayout->setContentsMargins(0, 0, 0, 0);
    versionLayout->addLayout(verticalLayout);

    auto *statusLayout = new QHBoxLayout(this);
    statusLayout->setSpacing(statusLayoutSpacing);
    _updateStatusLabel = new QLabel(this);
    _updateStatusLabel->setObjectName("boldTextLabel");
    statusLayout->addWidget(_updateStatusLabel);

    _betaTag = new TagLabel(betaTagColor, this);
    _betaTag->setText("BETA");
    _betaTag->setVisible(false);
    statusLayout->addWidget(_betaTag);
    statusLayout->addStretch();
    verticalLayout->addLayout(statusLayout);

    _showReleaseNotesLabel = new QLabel(this);
    _showReleaseNotesLabel->setObjectName("boldTextLabel");
    _showReleaseNotesLabel->setWordWrap(true);
    _showReleaseNotesLabel->setVisible(false);
    verticalLayout->addWidget(_showReleaseNotesLabel);

    static const QString versionNumberLinkText =
            tr(R"(<a style="%1" href="%2">%3</a>)").arg(CommonUtility::linkStyle, versionLink, KDRIVE_VERSION_STRING);
    _versionNumberLabel = new QLabel(this);
    _versionNumberLabel->setContextMenuPolicy(Qt::PreventContextMenu);
    _versionNumberLabel->setText(versionNumberLinkText);
    verticalLayout->addWidget(_versionNumberLabel);

    const auto copyrightLabel = new QLabel(QString("Copyright %1").arg(APPLICATION_VENDOR));
    copyrightLabel->setObjectName("description");
    verticalLayout->addWidget(copyrightLabel);

    _updateButton = new QPushButton(this);
    _updateButton->setObjectName("defaultbutton");
    _updateButton->setFlat(true);
    versionLayout->addWidget(_updateButton);
}

void VersionWidget::initBetaBloc(PreferencesBlocWidget *prefBloc) {
#if defined(KD_LINUX)
    return; // Beta program is not available on Linux for now
#endif

    auto *betaLayout = prefBloc->addLayout(QBoxLayout::Direction::LeftToRight);

    auto *verticalLayout = new QVBoxLayout(this);
    verticalLayout->setSpacing(1);
    verticalLayout->setContentsMargins(0, 0, 0, 0);
    betaLayout->addLayout(verticalLayout);

    _betaVersionLabel = new QLabel(this);
    _betaVersionLabel->setObjectName("boldTextLabel");
    _betaVersionLabel->setWordWrap(true);
    verticalLayout->addWidget(_betaVersionLabel);

    _betaVersionDescription = new QLabel(this);
    _betaVersionDescription->setObjectName("description");
    _betaVersionDescription->setWordWrap(true);
    _betaVersionDescription->setMinimumWidth(300);
    verticalLayout->addWidget(_betaVersionDescription);

    betaLayout->addStretch();

    _joinBetaButton = new QPushButton(this);
    _joinBetaButton->setObjectName("transparentbutton");
    _joinBetaButton->setFlat(true);
    betaLayout->addWidget(_joinBetaButton);
}

void VersionWidget::saveDistributionChannel(const VersionChannel channel) const {
    GuiRequests::changeDistributionChannel(channel);
    ParametersCache::instance()->parametersInfo().setDistributionChannel(channel);
    ParametersCache::instance()->saveParametersInfo();
}

} // namespace KDC
