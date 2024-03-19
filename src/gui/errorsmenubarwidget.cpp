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

#include "errorsmenubarwidget.h"
#include "customtoolbutton.h"
#include "guiutility.h"
#include "languagechangefilter.h"
#include "clientgui.h"

#include <QLoggingCategory>

namespace KDC {

static const int hMargin = 15;
static const int vMargin = 15;
static const int hButtonsSpacing = 10;
static const int driveLogoIconSize = 24;

Q_LOGGING_CATEGORY(lcErrorsMenuBarWidget, "gui.errorsmenubarwidget", QtInfoMsg)

ErrorsMenuBarWidget::ErrorsMenuBarWidget(std::shared_ptr<ClientGui> gui, QWidget *parent)
    : HalfRoundRectWidget(parent),
      _gui(gui),
      _driveDbId(0),
      _backButton(nullptr),
      _driveIconLabel(nullptr),
      _titleLabel(nullptr) {
    setContentsMargins(hMargin, 0, hMargin, vMargin);
    setSpacing(0);

    _backButton = new CustomToolButton(this);
    _backButton->setIconPath(":/client/resources/icons/actions/arrow-left.svg");
    addWidget(_backButton);

    addSpacing(hButtonsSpacing);

    _driveIconLabel = new QLabel(this);
    addWidget(_driveIconLabel);

    addSpacing(hButtonsSpacing);

    _titleLabel = new QLabel(this);
    _titleLabel->setObjectName("titleLabel");

    addWidget(_titleLabel);
    addStretch();

    // Init labels and setup connection for on the fly translation
    retranslateUi();
    LanguageChangeFilter *languageFilter = new LanguageChangeFilter(this);
    installEventFilter(languageFilter);
    connect(languageFilter, &LanguageChangeFilter::retranslate, this, &ErrorsMenuBarWidget::retranslateUi);

    connect(_backButton, &CustomToolButton::clicked, this, &ErrorsMenuBarWidget::onBackButtonClicked);
}

void ErrorsMenuBarWidget::setDrive(int driveDbId) {
    if (driveDbId) {
        const auto driveInfoIt = _gui->driveInfoMap().find(driveDbId);
        if (driveInfoIt == _gui->driveInfoMap().end()) {
            qCWarning(lcErrorsMenuBarWidget()) << "Drive not found in drive map for driveDbId=" << driveDbId;
            return;
        }

        _driveDbId = driveDbId;
        _driveIconLabel->setPixmap(
            KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/drive.svg", driveInfoIt->second.color())
                .pixmap(QSize(driveLogoIconSize, driveLogoIconSize)));
        _titleLabel->setText(tr("Synchronization conflicts or errors"));
    } else {
        _titleLabel->setText(tr("Errors"));
    }
}

void ErrorsMenuBarWidget::reset() {
    _driveDbId = 0;
}

void ErrorsMenuBarWidget::onBackButtonClicked(bool checked) {
    Q_UNUSED(checked)

    emit backButtonClicked();
}

void ErrorsMenuBarWidget::retranslateUi() {
    _backButton->setToolTip(tr("Back to preferences"));
    if (_driveDbId) {
        _titleLabel->setText(tr("Synchronization conflicts or errors"));
    } else {
        _titleLabel->setText(tr("Errors"));
    }
}

}  // namespace KDC
