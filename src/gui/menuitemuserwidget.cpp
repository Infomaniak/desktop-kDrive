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

#include "menuitemuserwidget.h"
#include "guiutility.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QStyleOption>

namespace KDC {

static const int boxSideMargin = 8;
static const int boxBottomMargin = 4;
static const int boxSpacing = 10;

static const int boxCurHMargin = 24;
static const int boxCurVMargin = 12;
static const int boxCurSpacing = 8;
static const int pictureUserSize = 24;
static const int pictureCurrUserSize = 40;

MenuItemUserWidget::MenuItemUserWidget(const QString &name, const QString &email, bool currentUser, QWidget *parent) :
    QWidget(parent), _name(name), _email(email), _leftPictureLabel(nullptr),
    _leftPictureSize(currentUser ? pictureCurrUserSize : pictureUserSize), _checked(false), _hasSubmenu(false) {
    if (currentUser) {
        // different view if it's current User
        initCurrentUserUI();
    } else {
        initUserUI();
    }
}


void MenuItemUserWidget::setLeftImage(const QImage &data) {
    _leftPictureLabel->setPixmap(KDC::GuiUtility::getAvatarFromImage(data).scaled(_leftPictureSize, _leftPictureSize,
                                                                                  Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

// return simple user margins
QMargins MenuItemUserWidget::getMargins() {
    return QMargins(boxSideMargin, boxSideMargin, boxSideMargin, boxBottomMargin);
}

void MenuItemUserWidget::initCurrentUserUI() {
    QVBoxLayout *mainlayout = new QVBoxLayout();
    mainlayout->setContentsMargins(boxCurHMargin, boxCurVMargin, boxCurHMargin, boxCurVMargin);

    _leftPictureLabel = new QLabel(this);
    mainlayout->addWidget(_leftPictureLabel);
    mainlayout->setAlignment(_leftPictureLabel, Qt::AlignHCenter);
    mainlayout->addSpacing(boxCurSpacing);

    QLabel *menuNameLabel = new QLabel(this);
    menuNameLabel->setObjectName("menuUserItemNameLabel");
    menuNameLabel->setText(_name);
    mainlayout->addWidget(menuNameLabel);
    mainlayout->setAlignment(menuNameLabel, Qt::AlignHCenter);
    mainlayout->setSpacing(0);

    QLabel *menuEmailLabel = new QLabel(this);
    menuEmailLabel->setObjectName("menuUserItemEmailLabel");
    menuEmailLabel->setText(_email);
    mainlayout->addWidget(menuEmailLabel);
    mainlayout->setAlignment(menuEmailLabel, Qt::AlignHCenter);

    mainlayout->addStretch();

    setLayout(mainlayout);
}

void MenuItemUserWidget::initUserUI() {
    setObjectName("selectableUser");
    QHBoxLayout *layout = new QHBoxLayout();
    layout->setContentsMargins(boxSideMargin, boxSideMargin, boxSideMargin, boxBottomMargin);
    layout->setSpacing(boxSpacing);

    _leftPictureLabel = new QLabel(this);
    layout->addWidget(_leftPictureLabel);
    layout->setSpacing(boxSpacing);

    QVBoxLayout *vText = new QVBoxLayout();
    QLabel *menuNameLabel = new QLabel(this);
    menuNameLabel->setObjectName("menuUserItemNameLabel");
    menuNameLabel->setText(_name);
    vText->addWidget(menuNameLabel);

    vText->setSpacing(0);
    QLabel *menuEmailLabel = new QLabel(this);
    menuEmailLabel->setObjectName("menuUserItemEmailLabel");
    menuEmailLabel->setText(_email);
    vText->addWidget(menuEmailLabel);

    layout->addLayout(vText);
    layout->addStretch();

    setLayout(layout);
}


void MenuItemUserWidget::paintEvent(QPaintEvent *paintEvent) {
    Q_UNUSED(paintEvent)

    QStyleOption opt;
    opt.initFrom(this);
    QPainter painter(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &painter, this);
}

} // namespace KDC
