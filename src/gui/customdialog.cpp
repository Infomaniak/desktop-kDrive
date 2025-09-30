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

#include "customdialog.h"
#include "customsystembar.h"
#include "guiutility.h"
#include "parameterscache.h"

#include <QBoxLayout>
#include <QGraphicsDropShadowEffect>
#include <QLabel>
#include <QLoggingCategory>
#include <QPainter>
#include <QPainterPath>
#include <QSizeGrip>
#include <QStylePainter>
#include <QTableView>
#include <QTimer>
#include <QVariant>

namespace KDC {

static const QSize windowMinimumSize(625, 530);
static const int cornerRadius = 5;
static const int hMargin = 20;
static const int vMargin = 20;
static const int mainBoxHMargin = 0;
static const int mainBoxVTMargin = 0;
static const int mainBoxVBMargin = 40;
static const int shadowBlurRadius = 20;
static const int resizeStripeWidth = 5;

Q_LOGGING_CATEGORY(lcCustomDialog, "gui.customdialog", QtInfoMsg)

CustomDialog::CustomDialog(const bool popup, QWidget *parent) :
    QDialog(parent),
    _backgroundColor(QColor()),
    _buttonIconColor(QColor()),
    _backgroundForcedColor(QColor()),
    _layout(nullptr) {
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_Hover, true);

    setMinimumSize(windowMinimumSize);

    installEventFilter(this);

    setContentsMargins(hMargin, vMargin, hMargin, vMargin);

    QVBoxLayout *mainVBox = new QVBoxLayout();
    mainVBox->setContentsMargins(0, 0, 0, 0);
    mainVBox->setSpacing(0);
    setLayout(mainVBox);

    // System bar
    CustomSystemBar *systemBar = nullptr;
    systemBar = new CustomSystemBar(popup, this);
    mainVBox->addWidget(systemBar);

    _layout = new QVBoxLayout();
    if (popup) {
        _layout->setContentsMargins(mainBoxHMargin, mainBoxVTMargin, mainBoxHMargin, mainBoxVBMargin);
        _layout->setSpacing(0);
    }

    mainVBox->addLayout(_layout);
    mainVBox->setStretchFactor(_layout, 1);

    // Shadow
    QGraphicsDropShadowEffect *effect = new QGraphicsDropShadowEffect(this);
    effect->setBlurRadius(shadowBlurRadius);
    effect->setOffset(0);
    setGraphicsEffect(effect);

    connect(systemBar, &CustomSystemBar::exit, this, &CustomDialog::onExit);
}

int CustomDialog::exec() {
    return QDialog::exec();
}

int CustomDialog::exec(QPoint position) {
    move(position);
    return exec();
}

int CustomDialog::execAndMoveToCenter(const QWidget *parent) {
    move(parent->pos() + QPoint((parent->width() - width()) / 2, (parent->height() - height()) / 2));
    return exec();
}

void CustomDialog::forceRedraw() {
#ifdef Q_OS_WINDOWS
    // Windows hack
    QTimer::singleShot(0, this, [=]() {
        if (geometry().height() > windowMinimumSize.height()) {
            setGeometry(geometry() + QMargins(0, 0, 0, -1));
            setMinimumHeight(windowMinimumSize.height());
        } else {
            setMinimumHeight(windowMinimumSize.height() + 1);
            setGeometry(geometry() + QMargins(0, 0, 0, 1));
        }
    });
#endif
}

void CustomDialog::setBackgroundForcedColor(const QColor &value) {
    _backgroundForcedColor = value;
    repaint();
}

bool CustomDialog::isResizable() const {
    return _isResizable;
}

void CustomDialog::setResizable(bool newIsResizable) {
    _isResizable = newIsResizable;
}

bool CustomDialog::eventFilter(QObject *o, QEvent *e) {
    switch (e->type()) {
        case QEvent::HoverMove: {
            mouseHover(static_cast<QHoverEvent *>(e));
            return true;
        }
        case QEvent::EnabledChange: {
            auto w = static_cast<QWidget *>(o);
            showDisabledOverlay(!w->isEnabled());
            return true;
        }
        default:
            break;
    }

    return QDialog::eventFilter(o, e);
}

void CustomDialog::drawRoundRectangle() {
    QPainterPath painterPath;
    painterPath.addRoundedRect(rect().marginsRemoved(QMargins(hMargin, vMargin, hMargin, vMargin)), cornerRadius, cornerRadius);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(_backgroundForcedColor == QColor() ? backgroundColor() : _backgroundForcedColor);
    painter.drawPath(painterPath);
}

void CustomDialog::drawDropShadow() {
    if (auto *effect = qobject_cast<QGraphicsDropShadowEffect *>(graphicsEffect()); effect) {
        effect->setColor(KDC::GuiUtility::getShadowColor(true));
    }
}

void CustomDialog::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)

    drawDropShadow(); // Can cause refresh issues on Windows.
    drawRoundRectangle();
}

void CustomDialog::mouseMoveEvent(QMouseEvent *event) {
    QDialog::mouseMoveEvent(event);
    if (!_isResizable) return;

    if (_resizeMode) {
        QPointF offsetVector = event->globalPosition() - _initCursorPos;
        QRect newGeometry = QRect(_initTopLeft, _initBottomRight);

        switch (_initEdge) {
            case KDC::CustomDialog::Left: {
                int topLeftX = _initTopLeft.x() + static_cast<int>(round(offsetVector.x()));
                if (_initBottomRight.x() - topLeftX < windowMinimumSize.width())
                    topLeftX = _initBottomRight.x() - windowMinimumSize.width();

                newGeometry = QRect(QPoint(topLeftX, _initTopLeft.y()), _initBottomRight);
                break;
            }
            case KDC::CustomDialog::TopLeft: {
                int topLeftX = _initTopLeft.x() + static_cast<int>(round(offsetVector.x()));
                if (_initBottomRight.x() - topLeftX < windowMinimumSize.width())
                    topLeftX = _initBottomRight.x() - windowMinimumSize.width();

                int topLeftY = _initTopLeft.y() + static_cast<int>(round(offsetVector.y()));
                if (_initBottomRight.y() - topLeftY < windowMinimumSize.height())
                    topLeftY = _initBottomRight.y() - windowMinimumSize.height();

                newGeometry = QRect(QPoint(topLeftX, topLeftY), _initBottomRight);
                break;
            }
            case KDC::CustomDialog::Top: {
                int topLeftY = _initTopLeft.y() + static_cast<int>(round(offsetVector.y()));
                if (_initBottomRight.y() - topLeftY < windowMinimumSize.height())
                    topLeftY = _initBottomRight.y() - windowMinimumSize.height();

                newGeometry = QRect(QPoint(_initTopLeft.x(), topLeftY), _initBottomRight);
                break;
            }
            case KDC::CustomDialog::TopRight: {
                int topLeftY = _initTopLeft.y() + static_cast<int>(round(offsetVector.y()));
                if (_initBottomRight.y() - topLeftY < windowMinimumSize.height())
                    topLeftY = _initBottomRight.y() - windowMinimumSize.height();

                int bottomRightX = _initBottomRight.x() + static_cast<int>(round(offsetVector.x()));
                if (bottomRightX - _initTopLeft.x() < windowMinimumSize.width()) bottomRightX = windowMinimumSize.width();

                newGeometry = QRect(QPoint(_initTopLeft.x(), topLeftY), QPoint(bottomRightX, _initBottomRight.y()));
                break;
            }
            case KDC::CustomDialog::Right: {
                int bottomRightX = _initBottomRight.x() + static_cast<int>(round(offsetVector.x()));
                if (bottomRightX - _initTopLeft.x() < windowMinimumSize.width()) bottomRightX = windowMinimumSize.width();

                newGeometry = QRect(_initTopLeft, QPoint(bottomRightX, _initBottomRight.y()));
                break;
            }
            case KDC::CustomDialog::BottomRight: {
                int bottomRightX = _initBottomRight.x() + static_cast<int>(round(offsetVector.x()));
                if (bottomRightX - _initTopLeft.x() < windowMinimumSize.width()) bottomRightX = windowMinimumSize.width();

                int bottomRightY = _initBottomRight.y() + static_cast<int>(round(offsetVector.y()));
                if (bottomRightY - _initTopLeft.y() < windowMinimumSize.height()) bottomRightY = windowMinimumSize.height();

                newGeometry = QRect(_initTopLeft, QPoint(bottomRightX, bottomRightY));
                break;
            }
            case KDC::CustomDialog::Bottom: {
                int bottomRightY = _initBottomRight.y() + static_cast<int>(round(offsetVector.y()));
                if (bottomRightY - _initTopLeft.y() < windowMinimumSize.height()) bottomRightY = windowMinimumSize.height();

                newGeometry = QRect(_initTopLeft, QPoint(_initBottomRight.x(), bottomRightY));
                break;
            }
            case KDC::CustomDialog::BottomLeft: {
                int topLeftX = _initTopLeft.x() + static_cast<int>(round(offsetVector.x()));
                if (_initBottomRight.x() - topLeftX < windowMinimumSize.width())
                    topLeftX = _initBottomRight.x() - windowMinimumSize.width();

                int bottomRightY = _initBottomRight.y() + static_cast<int>(round(offsetVector.y()));
                if (bottomRightY - _initTopLeft.y() < windowMinimumSize.height()) bottomRightY = windowMinimumSize.height();

                newGeometry = QRect(QPoint(topLeftX, _initTopLeft.y()), QPoint(_initBottomRight.x(), bottomRightY));
                break;
            }
            case KDC::CustomDialog::None:
            default:
                break;
        }

        setGeometry(newGeometry);
    }
}

void CustomDialog::mousePressEvent(QMouseEvent *event) {
    QDialog::mousePressEvent(event);
    if (!_isResizable) return;

    Edge edge = calculateCursorPosition(event->pos().x(), event->pos().y());
    if (edge != None) {
        _resizeMode = true;
        _initCursorPos = event->globalPosition();
        _initTopLeft = geometry().topLeft();
        _initBottomRight = geometry().bottomRight();
        _initEdge = edge;
    }
}

void CustomDialog::mouseReleaseEvent(QMouseEvent *event) {
    QDialog::mouseReleaseEvent(event);
    if (!_isResizable) return;

    _resizeMode = false;
    ParametersCache::instance()->parametersInfo().setDialogGeometry(this->objectName(), this->saveGeometry());
    ParametersCache::instance()->saveParametersInfo(false);
}

void CustomDialog::mouseHover(QHoverEvent *event) {
    if (!_isResizable) return;

    if (_resizeMode) return; // Do not change cursor in resize mode

    switch (calculateCursorPosition(static_cast<int>(round(event->position().x())),
                                    static_cast<int>(round(event->position().y())))) {
        case KDC::CustomDialog::Left:
            setCursor(Qt::SizeHorCursor);
            break;
        case KDC::CustomDialog::TopLeft:
            setCursor(Qt::SizeFDiagCursor);
            break;
        case KDC::CustomDialog::Top:
            setCursor(Qt::SizeVerCursor);
            break;
        case KDC::CustomDialog::TopRight:
            setCursor(Qt::SizeBDiagCursor);
            break;
        case KDC::CustomDialog::Right:
            setCursor(Qt::SizeHorCursor);
            break;
        case KDC::CustomDialog::BottomRight:
            setCursor(Qt::SizeFDiagCursor);
            break;
        case KDC::CustomDialog::Bottom:
            setCursor(Qt::SizeVerCursor);
            break;
        case KDC::CustomDialog::BottomLeft:
            setCursor(Qt::SizeBDiagCursor);
            break;
        case KDC::CustomDialog::None:
        default:
            setCursor(Qt::ArrowCursor);
            break;
    }
}

void CustomDialog::showDisabledOverlay(bool showOverlay) {
    if (showOverlay) {
        if (_disabledOverlay) {
            delete _disabledOverlay;
        }
        _disabledOverlay = new DisabledOverlay(this);
        _disabledOverlay->setGeometry(geometry());
        _disabledOverlay->show();
    } else {
        _disabledOverlay->hide();
    }
}

CustomDialog::Edge CustomDialog::calculateCursorPosition(int x, int y) {
    if (x > hMargin - resizeStripeWidth && x < hMargin + resizeStripeWidth) {
        if (y > vMargin - resizeStripeWidth && y < vMargin + resizeStripeWidth)
            return TopLeft;
        else if (y > size().height() - vMargin - resizeStripeWidth && y < size().height() - vMargin + resizeStripeWidth)
            return BottomLeft;
        else
            return Left;
    } else if (x > size().width() - hMargin - resizeStripeWidth && x < size().width() - hMargin + resizeStripeWidth) {
        if (y > vMargin - resizeStripeWidth && y < vMargin + resizeStripeWidth)
            return TopRight;
        else if (y > size().height() - vMargin - resizeStripeWidth && y < size().height() - vMargin + resizeStripeWidth)
            return BottomRight;
        else
            return Right;
    } else if (y > vMargin - resizeStripeWidth && y < vMargin + resizeStripeWidth) {
        return Top;
    } else if (y > size().height() - vMargin - resizeStripeWidth && y < size().height() - vMargin + resizeStripeWidth) {
        return Bottom;
    }

    return None;
}

void CustomDialog::onExit() {
    emit exit();
}

} // namespace KDC
