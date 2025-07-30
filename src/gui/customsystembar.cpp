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

#include "customsystembar.h"
#include "customtoolbutton.h"

#include <QBoxLayout>
#include <QApplication>
#include <QIcon>
#include <QStyle>
#include <QWindow>

namespace KDC {

static const int popupBarHeight = 38;
static const int popupBoxHMargin = 20;
static const int popupBoxVTMargin = 18;
static const int popupBoxVBMargin = 2;

static const int macDialogBarHeight = 30;
static const int macDialogBoxHMargin = 8;
static const int macDialogBoxVMargin = 8;
static const int macDialogBoxSpacing = 5;
static const QSize macIconSize = QSize(12, 12);

static const int winDialogBarHeight = 30;
static const int winDialogBoxHMargin = 12;
static const int winDialogBoxVTMargin = 10;
static const int winDialogBoxVBMargin = 0;

CustomSystemBar::CustomSystemBar(bool popup, QWidget *parent) :
    QWidget(parent),
    _popup(popup) {
    QHBoxLayout *hBox = new QHBoxLayout();
    setLayout(hBox);

    if (_popup) {
        setMinimumHeight(popupBarHeight);
        setMaximumHeight(popupBarHeight);
        hBox->setContentsMargins(popupBoxHMargin, popupBoxVTMargin, popupBoxHMargin, popupBoxVBMargin);

        CustomToolButton *exitButton = new CustomToolButton(this);
        exitButton->setObjectName("exitBigButton");
        exitButton->setIconPath(":/client/resources/icons/actions/close.svg");
        hBox->addStretch();
        hBox->addWidget(exitButton);

        connect(exitButton, &CustomToolButton::clicked, this, &CustomSystemBar::onExit);
    } else {
        if (CommonUtility::isMac()) {
            setMinimumHeight(macDialogBarHeight);
            setMaximumHeight(macDialogBarHeight);
            hBox->setContentsMargins(macDialogBoxHMargin, macDialogBoxVMargin, macDialogBoxHMargin, macDialogBoxVMargin);
            hBox->setSpacing(macDialogBoxSpacing);

            QIcon closeIcon;
            closeIcon.addFile(":/client/resources/icons/mac/dark/close.svg", QSize(), QIcon::Normal);
            closeIcon.addFile(":/client/resources/icons/mac/dark/close-hover.svg", QSize(), QIcon::Active);
            closeIcon.addFile(":/client/resources/icons/mac/dark/unactive.svg", QSize(), QIcon::Disabled);
            QToolButton *closeButton = new QToolButton(this);
            closeButton->setIcon(closeIcon);
            closeButton->setIconSize(macIconSize);
            closeButton->setAutoRaise(true);
            hBox->addWidget(closeButton);

            QIcon minIcon;
            minIcon.addFile(":/client/resources/icons/mac/dark/unactive.svg", QSize(), QIcon::Disabled);
            QToolButton *minButton = new QToolButton(this);
            minButton->setIcon(minIcon);
            minButton->setIconSize(macIconSize);
            minButton->setEnabled(false);
            hBox->addWidget(minButton);

            QIcon maxIcon;
            maxIcon.addFile(":/client/resources/icons/mac/dark/unactive.svg", QSize(), QIcon::Disabled);
            QToolButton *maxButton = new QToolButton(this);
            maxButton->setIcon(maxIcon);
            maxButton->setIconSize(macIconSize);
            maxButton->setEnabled(false);
            hBox->addWidget(maxButton);
            hBox->addStretch();

            connect(closeButton, &QToolButton::clicked, this, &CustomSystemBar::onExit);
        } else {
            setMinimumHeight(winDialogBarHeight);
            setMaximumHeight(winDialogBarHeight);
            hBox->setContentsMargins(winDialogBoxHMargin, winDialogBoxVTMargin, winDialogBoxHMargin, winDialogBoxVBMargin);

            CustomToolButton *closeButton = new CustomToolButton(this);
            closeButton->setObjectName("exitLittleButton");
            closeButton->setIconPath(":/client/resources/icons/actions/close.svg");
            hBox->addStretch();
            hBox->addWidget(closeButton);

            connect(closeButton, &CustomToolButton::clicked, this, &CustomSystemBar::onExit);
        }
    }
}

void CustomSystemBar::mousePressEvent(QMouseEvent *event) {
    if (event->buttons() == Qt::LeftButton) {
        (void) window()->windowHandle()->startSystemMove();
    }
    QWidget::mousePressEvent(event);
}

bool CustomSystemBar::event(QEvent *event) {
    if (event->type() == QEvent::WindowActivate || event->type() == QEvent::WindowDeactivate) {
        QList<QToolButton *> buttonList = findChildren<QToolButton *>();
        for (QToolButton *button: buttonList) {
            button->setEnabled(event->type() == QEvent::WindowActivate ? true : false);
        }
    }
    return QWidget::event(event);
}

void CustomSystemBar::onExit(bool checked) {
    Q_UNUSED(checked)

    emit exit();
}

} // namespace KDC
