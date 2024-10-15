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

#include "versionwidget.h"

#include "aboutdialog.h"
#include "enablestateholder.h"
#include "guirequests.h"
#include "guiutility.h"
#include "parameterscache.h"
#include "preferencesblocwidget.h"
#include "utility/utility.h"
#include "utility/widgetsignalblocker.h"

#include <QDesktopServices>
#include <config.h>

#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>
#include <version.h>

namespace KDC {

static const QString versionLink = "versionLink";
static const QString releaseNoteLink = "releaseNoteLink";
static const QString downloadPageLink = "downloadPageLink";

VersionWidget::VersionWidget(QWidget *parent /*= nullptr*/) : QWidget(parent) {
    const auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    setLayout(mainLayout);

    _versionLabel = new QLabel(this);
    _versionLabel->setObjectName("blocLabel");
    layout()->addWidget(_versionLabel);

    const auto versionBloc = new PreferencesBlocWidget();
    layout()->addWidget(versionBloc);
    QBoxLayout *versionBox = versionBloc->addLayout(QBoxLayout::Direction::LeftToRight);
    const auto versionVBox = new QVBoxLayout();
    versionVBox->setContentsMargins(0, 0, 0, 0);
    versionVBox->setSpacing(1);
    versionBox->addLayout(versionVBox);
    versionBox->setStretchFactor(versionVBox, 1);

    _updateStatusLabel = new QLabel(this);
    _updateStatusLabel->setObjectName("boldTextLabel");
    _updateStatusLabel->setWordWrap(true);
    versionVBox->addWidget(_updateStatusLabel);

    // TODO : add it back later
    // const auto channelBox = new QHBoxLayout(this);
    // _prodButton = new QRadioButton(tr("Prod"), this);
    // channelBox->addWidget(_prodButton);
    // channelBox->addStretch();
    // _betaButton = new QRadioButton(tr("Beta"), this);
    // channelBox->addWidget(_betaButton);
    // channelBox->addStretch();
    // _internalButton = new QRadioButton(tr("Internal"), this);
    // channelBox->addWidget(_internalButton);
    // channelBox->addStretch();
    // versionVBox->addLayout(channelBox);

    _showReleaseNoteLabel = new QLabel(this);
    _showReleaseNoteLabel->setObjectName("boldTextLabel");
    _showReleaseNoteLabel->setWordWrap(true);
    _showReleaseNoteLabel->setVisible(false);
    versionVBox->addWidget(_showReleaseNoteLabel);

    static const QString versionNumberLinkText =
            tr(R"(<a style="%1" href="%2">%3</a>)").arg(CommonUtility::linkStyle, versionLink, KDRIVE_VERSION_STRING);
    _versionNumberLabel = new QLabel(this);
    _versionNumberLabel->setContextMenuPolicy(Qt::PreventContextMenu);
    _versionNumberLabel->setText(versionNumberLinkText);
    versionVBox->addWidget(_versionNumberLabel);

    const auto copyrightLabel = new QLabel(QString("Copyright %1").arg(APPLICATION_VENDOR));
    copyrightLabel->setObjectName("description");
    versionVBox->addWidget(copyrightLabel);

    _updateButton = new QPushButton(this);
    _updateButton->setObjectName("defaultbutton");
    _updateButton->setFlat(true);
    versionBox->addWidget(_updateButton);

    refresh();

    // connect(_prodButton, &QRadioButton::clicked, this, &VersionWidget::onChannelButtonClicked);
    // connect(_betaButton, &QRadioButton::clicked, this, &VersionWidget::onChannelButtonClicked);
    // connect(_internalButton, &QRadioButton::clicked, this, &VersionWidget::onChannelButtonClicked);
    connect(_updateStatusLabel, &QLabel::linkActivated, this, &VersionWidget::onLinkActivated);
    connect(_versionNumberLabel, &QLabel::linkActivated, this, &VersionWidget::onLinkActivated);
    connect(_showReleaseNoteLabel, &QLabel::linkActivated, this, &VersionWidget::onLinkActivated);
    connect(_updateButton, &QPushButton::clicked, this, &VersionWidget::onUpdatButtonClicked);
}

void VersionWidget::refresh(UpdateState state /*= UpdateStateV2::Unknown*/) const {
    // Re-translate
    const QString releaseNoteLinkText =
            tr(R"(<a style="%1" href="%2">Show release note</a>)").arg(CommonUtility::linkStyle, releaseNoteLink);
    _showReleaseNoteLabel->setText(releaseNoteLinkText);

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
        case UpdateState::Ready:
        case UpdateState::Skipped: {
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
    }

    _updateStatusLabel->setText(statusString);
    _showReleaseNoteLabel->setVisible(showReleaseNote);
    _updateButton->setVisible(showUpdateButton);
}

void VersionWidget::showAboutDialog() {
    EnableStateHolder _(this);
    AboutDialog dialog(this);
    dialog.execAndMoveToCenter(GuiUtility::getTopLevelWidget(this));
}

void VersionWidget::showReleaseNote() const {
    QString os;
#if defined(__APPLE__)
    os = ""; // In order to works with Sparkle, the URL must have the same name as the package. So do not add the os for macOS
#elif defined(_WIN32)
    os = "-win";
#else
    os = "-linux";
#endif

    VersionInfo versionInfo;
    GuiRequests::versionInfo(versionInfo);

    const Language &appLanguage = ParametersCache::instance()->parametersInfo().language();
    if (const QString languageCode = CommonUtility::languageCode(appLanguage);
        CommonUtility::languageCodeIsEnglish(languageCode)) {
        QDesktopServices::openUrl(QUrl(QString("%1-%2%3.html").arg(APPLICATION_STORAGE_URL, versionInfo.tag.c_str(), os)));
    } else {
        QDesktopServices::openUrl(
                QUrl(QString("%1-%2%3-%4.html").arg(APPLICATION_STORAGE_URL, versionInfo.tag.c_str(), os, languageCode)));
    }
}

void VersionWidget::showDownloadPage() const {
    QDesktopServices::openUrl(QUrl(APPLICATION_DOWNLOAD_URL));
}

void VersionWidget::onUpdateStateChanged(const UpdateState state) const {
    refresh(state);
}

void VersionWidget::onChannelButtonClicked() const {
    auto channel = DistributionChannel::Unknown;
    if (sender() == _prodButton)
        channel = DistributionChannel::Prod;
    else if (sender() == _betaButton)
        channel = DistributionChannel::Beta;
    else if (sender() == _internalButton)
        channel = DistributionChannel::Internal;
    else
        return;

    GuiRequests::changeDistributionChannel(channel);
    refresh();
}

void VersionWidget::onLinkActivated(const QString &link) {
    if (link == versionLink)
        showAboutDialog();
    else if (link == releaseNoteLink)
        showReleaseNote();
    else if (link == downloadPageLink)
        showDownloadPage();
}

void VersionWidget::onUpdatButtonClicked() {
#if defined(__APPLE__)
    GuiRequests::startInstaller();
#else
    VersionInfo versionInfo;
    GuiRequests::versionInfo(versionInfo);
    emit showUpdateDialog(versionInfo);
#endif
}

void VersionWidget::refreshChannelButtons(const DistributionChannel channel) const {
    switch (channel) {
        case DistributionChannel::Prod: {
            WidgetSignalBlocker _(_prodButton);
            _prodButton->setChecked(true);
            break;
        }
        case DistributionChannel::Beta: {
            WidgetSignalBlocker _(_betaButton);
            _betaButton->setChecked(true);
            break;
        }
        case DistributionChannel::Internal: {
            WidgetSignalBlocker _(_internalButton);
            _internalButton->setChecked(true);
            break;
        }
        default:
            break;
    }
}

} // namespace KDC
