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

#include "errorspopup.h"
#include "clickablewidget.h"
#include "guiutility.h"

#include <QBoxLayout>
#include <QGraphicsDropShadowEffect>
#include <QLabel>
#include <QLoggingCategory>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QTimer>

namespace KDC {

static const int minHeight = 50;
static const int windowWidth = 310;
static const int cornerRadius = 5;
static const int hMargin = 10;
static const int vMargin = 10;
static const int hSpacing = 0;
static const int vSpacing = 10;
static const int boxHMargin = 15;
static const int boxVMargin = 15;
static const int shadowBlurRadius = 20;
static const int menuOffsetX = -30;
static const int menuOffsetY = 10;

static const std::string actionTypeProperty = "actionType";

Q_LOGGING_CATEGORY(lcErrorsPopup, "gui.errorspopup", QtInfoMsg)

ErrorsPopup::ErrorsPopup(const QList<DriveError> &driveErrorList, int genericErrorsCount, QPoint position, QWidget *parent) :
    QDialog(parent), _moved(false), _position(position), _backgroundColor(QColor()), _warningIconSize(QSize()),
    _warningIconColor(QColor()) {
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint | Qt::X11BypassWindowManagerHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);

    setMinimumWidth(windowWidth);
    setMaximumWidth(windowWidth);

    setContentsMargins(hMargin, vMargin, hMargin, vMargin);

    QVBoxLayout *mainVBox = new QVBoxLayout();
    mainVBox->setContentsMargins(boxHMargin, boxVMargin, boxHMargin, boxVMargin);
    mainVBox->setSpacing(vSpacing);
    setLayout(mainVBox);

    // Title
    QLabel *titleLabel = new QLabel(this);
    titleLabel->setMinimumHeight(minHeight);
    titleLabel->setText(tr("Some files couldn't be synchronized on the following kDrive(s) :"));
    titleLabel->setWordWrap(true);
    mainVBox->addWidget(titleLabel);

    // Drive errors
    for (auto const &driveError: driveErrorList) {
        ClickableWidget *driveWidget = new ClickableWidget(this);
        driveWidget->setProperty(actionTypeProperty.c_str(), driveError.driveDbId);
        mainVBox->addWidget(driveWidget);

        QHBoxLayout *driveErrorHBox = new QHBoxLayout();
        driveErrorHBox->setContentsMargins(0, 0, 0, 0);
        driveErrorHBox->addSpacing(hSpacing);
        driveWidget->setLayout(driveErrorHBox);

        QLabel *driveNameLabel = new QLabel(this);
        QString text;
        if (driveError.unresolvedErrorsCount > 0 && driveError.autoresolvedErrorsCount == 0) {
            QLabel *warningIconLabel = new QLabel(this);
            warningIconLabel->setObjectName("warningIconLabel");
            warningIconLabel->setWordWrap(true);
            driveErrorHBox->addWidget(warningIconLabel);
            text = driveError.driveName + QString(tr(" (%1 error(s))").arg(driveError.unresolvedErrorsCount));
        } else if (driveError.autoresolvedErrorsCount > 0 && driveError.unresolvedErrorsCount == 0) {
            QLabel *infoIconLabel = new QLabel(this);
            infoIconLabel->setObjectName("infoIconLabel");
            infoIconLabel->setWordWrap(true);
            driveErrorHBox->addWidget(infoIconLabel);
            text = driveError.driveName + QString(tr(" (%1 information(s))").arg(driveError.autoresolvedErrorsCount));
        } else {
            QLabel *warningIconLabel = new QLabel(this);
            warningIconLabel->setObjectName("warningIconLabel");
            warningIconLabel->setWordWrap(true);
            driveErrorHBox->addWidget(warningIconLabel);
            text = driveError.driveName + QString(tr(" (%1 error(s) and %2 information(s))")
                                                          .arg(driveError.unresolvedErrorsCount)
                                                          .arg(driveError.autoresolvedErrorsCount));
        }

        driveNameLabel->setText(text);
        driveNameLabel->setWordWrap(true);
        driveErrorHBox->addWidget(driveNameLabel);
        driveErrorHBox->addStretch();

        QLabel *arrowIconLabel = new QLabel(this);
        arrowIconLabel->setObjectName("arrowIconLabel");
        driveErrorHBox->addWidget(arrowIconLabel);

        connect(driveWidget, &ClickableWidget::clicked, this, &ErrorsPopup::onActionButtonClicked);
    }

    // Generic errors
    if (driveErrorList.count() > 0 && genericErrorsCount > 0) {
        // Add separator
        QFrame *line = new QFrame();
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);
        mainVBox->addWidget(line);
    }

    if (genericErrorsCount > 0) {
        ClickableWidget *genericErrorsWidget = new ClickableWidget(this);
        mainVBox->addWidget(genericErrorsWidget);

        QHBoxLayout *errorHBox = new QHBoxLayout();
        errorHBox->setContentsMargins(0, 0, 0, 0);
        errorHBox->addSpacing(hSpacing);
        genericErrorsWidget->setLayout(errorHBox);

        QLabel *warningIconLabel = new QLabel(this);
        warningIconLabel->setObjectName("warningIconLabel");
        errorHBox->addWidget(warningIconLabel);

        QLabel *driveNameLabel = new QLabel(this);
        QString text =
                QString(tr("Generic errors (%n warning(s) or error(s))", "Number of warnings or errors", genericErrorsCount));
        driveNameLabel->setText(text);
        driveNameLabel->setWordWrap(true);
        errorHBox->addWidget(driveNameLabel);
        errorHBox->addStretch();

        QLabel *arrowIconLabel = new QLabel(this);
        arrowIconLabel->setObjectName("arrowIconLabel");
        errorHBox->addWidget(arrowIconLabel);

        connect(genericErrorsWidget, &ClickableWidget::clicked, this, &ErrorsPopup::onActionButtonClicked);
    }

    // Shadow
    QGraphicsDropShadowEffect *effect = new QGraphicsDropShadowEffect(this);
    effect->setBlurRadius(shadowBlurRadius);
    effect->setOffset(0);
    setGraphicsEffect(effect);
}

void ErrorsPopup::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)

    if (!_moved) {
        // Move menu
        _moved = true;
        QScreen *screen = QGuiApplication::screenAt(_position);
        Q_CHECK_PTR(screen);
        QRect displayRect = screen->geometry();
        int delta = displayRect.right() - (_position.x() + menuOffsetX + geometry().width());
        QPoint offset = QPoint(menuOffsetX + (delta > 0 ? 0 : delta), menuOffsetY);
        move(_position + offset);
    }

    // Update shadow color
    QGraphicsDropShadowEffect *effect = qobject_cast<QGraphicsDropShadowEffect *>(graphicsEffect());
    if (effect) {
        effect->setColor(KDC::GuiUtility::getShadowColor(true));
    }

    // Draw round rectangle
    QPainterPath painterPath;
    painterPath.addRoundedRect(rect().marginsRemoved(QMargins(hMargin, vMargin, hMargin, vMargin)), cornerRadius, cornerRadius);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(backgroundColor());
    painter.drawPath(painterPath);
}

void ErrorsPopup::setWarningIcon() {
    if (_warningIconSize != QSize() && _warningIconColor != QColor()) {
        QList<QLabel *> labelList = findChildren<QLabel *>("warningIconLabel");
        for (QLabel *label: labelList) {
            label->setPixmap(KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/warning.svg", _warningIconColor)
                                     .pixmap(_warningIconSize));
        }
    }
}

void ErrorsPopup::setInfoIcon() {
    if (_infoIconSize != QSize() && _infoIconColor != QColor()) {
        QList<QLabel *> labelList = findChildren<QLabel *>("infoIconLabel");
        for (QLabel *label: labelList) {
            label->setPixmap(KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/information.svg", _infoIconColor)
                                     .pixmap(_infoIconSize));
        }
    }
}

void ErrorsPopup::setArrowIcon() {
    if (_arrowIconSize != QSize() && _arrowIconColor != QColor()) {
        QList<QLabel *> labelList = findChildren<QLabel *>("arrowIconLabel");
        for (QLabel *label: labelList) {
            label->setPixmap(
                    KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/arrow-right.svg", _arrowIconColor)
                            .pixmap(_arrowIconSize));
        }
    }
}

void ErrorsPopup::onActionButtonClicked() {
    QString accountIdStr = qvariant_cast<QString>(sender()->property(actionTypeProperty.c_str()));
    int accountId = accountIdStr.toInt();
    QTimer::singleShot(0, this, [this, accountId]() { emit accountSelected(accountId); });
    done(QDialog::Accepted);
}

} // namespace KDC
