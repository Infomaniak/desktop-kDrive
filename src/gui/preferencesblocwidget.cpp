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

#include "guiutility.h"
#include "folderitemwidget.h"
#include "foldertreeitemwidget.h"
#include "guiutility.h"
#include "preferencesblocwidget.h"

#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QLoggingCategory>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QPainterPath>

namespace KDC {

static const int cornerRadius = 10;
static const int boxHMargin = 15;
static const int boxVMargin = 18;
static const int textVSpacing = 10;
static const int textHSpacing = 10;
static const int shadowBlurRadius = 20;

Q_LOGGING_CATEGORY(lcPreferencesBlocWidget, "gui.preferencesblocwidget", QtInfoMsg)

PreferencesBlocWidget::PreferencesBlocWidget(QWidget *parent) : LargeWidgetWithCustomToolTip(parent) {
    setContentsMargins(0, 0, 0, 0);

    _layout = new QVBoxLayout();
    _layout->setContentsMargins(0, 0, 0, 0);
    _layout->setSpacing(0);
    setLayout(_layout);

    // Shadow
    QGraphicsDropShadowEffect *effect = new QGraphicsDropShadowEffect(this);
    effect->setBlurRadius(shadowBlurRadius);
    effect->setOffset(0);
    setGraphicsEffect(effect);

    connect(this, &PreferencesBlocWidget::actionIconColorChanged, this, &PreferencesBlocWidget::onActionIconColorChanged);
    connect(this, &PreferencesBlocWidget::actionIconSizeChanged, this, &PreferencesBlocWidget::onActionIconSizeChanged);
}

QBoxLayout *PreferencesBlocWidget::addLayout(QBoxLayout::Direction direction, bool noMargins) {
    QBoxLayout *layout = new QBoxLayout(direction);
    if (!noMargins) {
        layout->setContentsMargins(boxHMargin, boxVMargin, boxHMargin, boxVMargin);
    }
    if (direction == QBoxLayout::Direction::TopToBottom || direction == QBoxLayout::Direction::BottomToTop) {
        layout->setSpacing(textVSpacing);
    } else {
        layout->setSpacing(textHSpacing);
    }
    _layout->addLayout(layout);

    return layout;
}

ClickableWidget *PreferencesBlocWidget::addActionWidget(QVBoxLayout **vLayout, bool noMargins) {
    ClickableWidget *widget = new ClickableWidget(this);
    widget->setContentsMargins(0, 0, 0, 0);
    _layout->addWidget(widget);

    QHBoxLayout *hLayout = new QHBoxLayout();
    if (!noMargins) {
        hLayout->setContentsMargins(boxHMargin, boxVMargin, boxHMargin, boxVMargin);
    }
    hLayout->setSpacing(0);
    widget->setLayout(hLayout);

    *vLayout = new QVBoxLayout();
    (*vLayout)->setContentsMargins(0, 0, 0, 0);
    (*vLayout)->setSpacing(textVSpacing);
    hLayout->addLayout(*vLayout);
    hLayout->addStretch();

    QLabel *actionIconLabel = new QLabel(this);
    actionIconLabel->setObjectName("actionIconLabel");
    hLayout->addWidget(actionIconLabel);

    return widget;
}

QFrame *PreferencesBlocWidget::addSeparator() {
    QFrame *line = new QFrame(this);
    line->setObjectName("line");
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Plain);
    _layout->addWidget(line);

    return line;
}

void PreferencesBlocWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    // Shadow
    QGraphicsDropShadowEffect *effect = qobject_cast<QGraphicsDropShadowEffect *>(graphicsEffect());
    if (effect && effect->color() != KDC::GuiUtility::getShadowColor()) {
        effect->setColor(KDC::GuiUtility::getShadowColor());
        effect->update();
    }

    // Draw background
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);

    // Draw round rectangle
    QPainterPath painterPath;
    painterPath.addRoundedRect(rect(), cornerRadius, cornerRadius);

    painter.setBrush(backgroundColor());
    painter.drawPath(painterPath);
}

void PreferencesBlocWidget::setActionIcon() {
    QList<QLabel *> allActionIconLabels = findChildren<QLabel *>("actionIconLabel");
    for (QLabel *actionIconLabel: allActionIconLabels) {
        actionIconLabel->setPixmap(
                KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/chevron-right.svg", _actionIconColor)
                        .pixmap(_actionIconSize));
    }
}

void PreferencesBlocWidget::onActionIconColorChanged() {
    setActionIcon();
}

void PreferencesBlocWidget::onActionIconSizeChanged() {
    setActionIcon();
}

void PreferencesBlocWidget::updateBloc() {
    if (auto folderItemWidget = findChild<FolderItemWidget *>(); folderItemWidget != nullptr) {
        if (!folderItemWidget->isBeingDeleted() && !isEnabled()) {
            // Re-enable the folder bloc if the synchronization deletion failed.
            setCustomToolTipText("");
            setEnabled(true);
        }
        folderItemWidget->updateItem();
    } else {
        qCDebug(lcPreferencesBlocWidget) << "Empty folder bloc!";
    }
}

void PreferencesBlocWidget::refreshFolders() const {
    if (auto folderTreeItemWidget = findChild<FolderTreeItemWidget *>();
        folderTreeItemWidget && folderTreeItemWidget->isVisible()) {
        folderTreeItemWidget->loadSubFolders();
    }
}

void PreferencesBlocWidget::setToolTipsEnabled(bool enabled) const {
    if (auto folderItemWidget = findChild<FolderItemWidget *>(); folderItemWidget != nullptr) {
        folderItemWidget->setToolTipsEnabled(enabled);
    } else {
        qCDebug(lcPreferencesBlocWidget) << "Empty folder bloc!";
    }
}

void PreferencesBlocWidget::setEnabledRecursively(bool enabled) {
    GuiUtility::setEnabledRecursively(this, enabled);
    setToolTipsEnabled(enabled);
    if (enabled) {
        setCustomToolTipText("");
    } else {
        setCustomToolTipText(tr("This synchronization is being deleted."));
    }
}

} // namespace KDC
