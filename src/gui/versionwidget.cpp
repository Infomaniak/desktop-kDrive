
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

#include "guirequests.h"
#include "preferencesblocwidget.h"
#include "utility/utility.h"

#include <config.h>

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <version.h>

namespace KDC {

static const QString versionLink = "versionLink";
static const QString releaseNoteLink = "releaseNoteLink";

VersionWidget::VersionWidget(QWidget *parent /*= nullptr*/) :
    QWidget(parent), _versionLabel{new QLabel()}, _updateStatusLabel{new QLabel()}, _showReleaseNoteLabel{new QLabel()},
    _versionNumberLabel{new QLabel()}, _updateButton{new QPushButton()} {
    const auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    setLayout(mainLayout);

    _versionLabel->setObjectName("blocLabel");
    layout()->addWidget(_versionLabel);

    const auto versionBloc = new PreferencesBlocWidget();
    layout()->addWidget(versionBloc);
    QBoxLayout *versionBox = versionBloc->addLayout(QBoxLayout::Direction::LeftToRight);
    const auto versionVBox = new QVBoxLayout();
    versionVBox->setContentsMargins(0, 0, 0, 0);
    versionVBox->setSpacing(0);
    versionBox->addLayout(versionVBox);
    versionBox->setStretchFactor(versionVBox, 1);

    // Status
    _updateStatusLabel->setObjectName("boldTextLabel");
    _updateStatusLabel->setWordWrap(true);
    _updateStatusLabel->setVisible(false);
    versionVBox->addWidget(_updateStatusLabel);

    _showReleaseNoteLabel->setObjectName("boldTextLabel");
    _showReleaseNoteLabel->setWordWrap(true);
    _showReleaseNoteLabel->setVisible(false);
    versionVBox->addWidget(_showReleaseNoteLabel);

    static const QString versionNumberLinkText =
            tr(R"(<a style="%1" href="%2">%3</a>)").arg(CommonUtility::linkStyle, versionLink, KDRIVE_VERSION_STRING);
    _versionNumberLabel->setContextMenuPolicy(Qt::PreventContextMenu);
    _versionNumberLabel->setText(versionNumberLinkText);
    versionVBox->addWidget(_versionNumberLabel);

    const auto copyrightLabel = new QLabel(QString("Copyright %1").arg(APPLICATION_VENDOR));
    copyrightLabel->setObjectName("description");
    versionVBox->addWidget(copyrightLabel);

    _updateButton->setObjectName("defaultbutton");
    _updateButton->setFlat(true);
    versionBox->addWidget(_updateButton);

    retranslate();

    connect(_versionNumberLabel, &QLabel::linkActivated, this, &VersionWidget::onLinkActivated);
    connect(_showReleaseNoteLabel, &QLabel::linkActivated, this, &VersionWidget::onLinkActivated);
    connect(_updateButton, &QPushButton::clicked, this, &VersionWidget::updateButtonClicked);
}

void VersionWidget::updateStatus(const QString &status, const bool updateAvailable) const {
    if (status.isEmpty()) {
        _updateStatusLabel->setVisible(false);
        _showReleaseNoteLabel->setVisible(false);
    } else {
        _updateStatusLabel->setVisible(true);
        _updateStatusLabel->setText(status);

        if (updateAvailable) {
            _showReleaseNoteLabel->setVisible(true);
        }
    }

    _updateButton->setVisible(updateAvailable);
}

void VersionWidget::refresh() const {
    // Re-translate
    const QString releaseNoteLinkText =
            tr(R"(<a style="%1" href="%2">Show release note</a>)").arg(CommonUtility::linkStyle, releaseNoteLink);
    _showReleaseNoteLabel->setText(releaseNoteLinkText);

    _versionLabel->setText(tr("Version"));
    _updateButton->setText(tr("UPDATE"));

    // Refresh update state
    auto state = UpdateStateV2::UpToDate;
    GuiRequests::updateState(state);
    switch (state) {
        case UpdateStateV2::UpToDate:
            break;
        case UpdateStateV2::Checking:
            break;
        case UpdateStateV2::Available:
            break;
        case UpdateStateV2::Downloading:
            break;
        case UpdateStateV2::Ready:
            break;
        case UpdateStateV2::Error:
            break;
    }
}

void VersionWidget::onLinkActivated(const QString &link) {
    if (link == versionLink)
        emit showAboutDialog();
    else if (link == releaseNoteLink)
        emit showReleaseNote();
}

} // namespace KDC
