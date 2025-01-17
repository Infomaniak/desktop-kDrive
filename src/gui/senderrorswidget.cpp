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

#include "senderrorswidget.h"
#include "guiutility.h"

#include <QHBoxLayout>
#include <QPainterPath>
#include <QPainter>

namespace KDC {

static const int boxHMargin = 20;
static const int boxVMargin = 5;
static const int boxSpacing = 10;

SendErrorsWidget::SendErrorsWidget(QWidget *parent) :
    ClickableWidget(parent), _backgroundColor(QColor()), _helpIconColor(QColor()), _helpIconSize(QSize()),
    _actionIconColor(QColor()), _actionIconSize(QSize()), _helpIconLabel(nullptr), _actionIconLabel(nullptr) {
    setContentsMargins(0, 0, 0, 0);

    QHBoxLayout *hbox = new QHBoxLayout();
    hbox->setContentsMargins(boxHMargin, boxVMargin, boxHMargin, boxVMargin);
    hbox->setSpacing(boxSpacing);
    setLayout(hbox);

    _helpIconLabel = new QLabel(this);
    hbox->addWidget(_helpIconLabel);

    QLabel *helpTextLabel = new QLabel(tr("Generate an archive of the application logs to send it to our support"), this);
    helpTextLabel->setObjectName("helpTextLabel");
    hbox->addWidget(helpTextLabel);
    hbox->addStretch();

    _actionIconLabel = new QLabel(this);
    hbox->addWidget(_actionIconLabel);

    connect(this, &SendErrorsWidget::helpIconSizeChanged, this, &SendErrorsWidget::onHelpIconSizeChanged);
    connect(this, &SendErrorsWidget::helpIconColorChanged, this, &SendErrorsWidget::onHelpIconColorChanged);
    connect(this, &SendErrorsWidget::actionIconColorChanged, this, &SendErrorsWidget::onActionIconColorChanged);
    connect(this, &SendErrorsWidget::actionIconSizeChanged, this, &SendErrorsWidget::onActionIconSizeChanged);
    connect(this, &ClickableWidget::clicked, this, &SendErrorsWidget::onClick);
}

void SendErrorsWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    // Draw round rectangle
    QPainterPath painterPath;
    painterPath.addRoundedRect(rect(), rect().height() / 2, rect().height() / 2);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(backgroundColor());
    painter.drawPath(painterPath);
}

void SendErrorsWidget::setHelpIcon() {
    if (_helpIconLabel && _helpIconSize != QSize() && _helpIconColor != QColor()) {
        _helpIconLabel->setPixmap(KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/help.svg", _helpIconColor)
                                          .pixmap(_helpIconSize));
    }
}

void SendErrorsWidget::setActionIcon() {
    if (_actionIconLabel && _actionIconSize != QSize() && _actionIconColor != QColor()) {
        _actionIconLabel->setPixmap(
                KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/chevron-right.svg", _actionIconColor)
                        .pixmap(_actionIconSize));
    }
}

void SendErrorsWidget::onHelpIconSizeChanged() {
    setHelpIcon();
}

void SendErrorsWidget::onHelpIconColorChanged() {
    setHelpIcon();
}

void SendErrorsWidget::onActionIconColorChanged() {
    setActionIcon();
}

void SendErrorsWidget::onActionIconSizeChanged() {
    setActionIcon();
}

void SendErrorsWidget::onClick() {
    emit displayHelp();
}

} // namespace KDC
