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

#include "languagechangefilter.h"
#include "preferencesmenubarwidget.h"

namespace KDC {

static const int hMargin = 15;
static const int vMargin = 15;
static const int hButtonsSpacing = 10;

PreferencesMenuBarWidget::PreferencesMenuBarWidget(QWidget *parent)
    : HalfRoundRectWidget(parent), _backButton(nullptr), _titleLabel(nullptr) {
    setContentsMargins(hMargin, 0, hMargin, vMargin);
    setSpacing(0);

    _backButton = new CustomToolButton(this);
    _backButton->setIconPath(":/client/resources/icons/actions/arrow-left.svg");
    addWidget(_backButton);

    addSpacing(hButtonsSpacing);

    _titleLabel = new QLabel(this);
    _titleLabel->setObjectName("titleLabel");
    addWidget(_titleLabel);

    addStretch();

    // Init labels and setup connection for on the fly translation
    retranslateUi();
    LanguageChangeFilter *languageFilter = new LanguageChangeFilter(this);
    installEventFilter(languageFilter);
    connect(languageFilter, &LanguageChangeFilter::retranslate, this, &PreferencesMenuBarWidget::retranslateUi);

    connect(_backButton, &CustomToolButton::clicked, this, &PreferencesMenuBarWidget::onBackButtonClicked);
}

void PreferencesMenuBarWidget::onBackButtonClicked(bool checked) {
    Q_UNUSED(checked)

    emit backButtonClicked();
}

void PreferencesMenuBarWidget::retranslateUi() {
    _backButton->setToolTip(tr("Back to drive list"));
    _titleLabel->setText(tr("Preferences"));
}

}  // namespace KDC
