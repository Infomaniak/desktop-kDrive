/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#include "windowdecorationcontroller.h"

#include <QGuiApplication>
#include <QRegion>
#include <QWindow>
#include <QtGui/qguiapplication_platform.h>

#include <cstdint>

#if QT_CONFIG(xcb)
#include <X11/Xlib.h>
#include <X11/extensions/shape.h>
#endif

namespace KDC {

namespace {

#if QT_CONFIG(xcb)
Display *x11Display() {
    if (QGuiApplication::platformName() != QStringLiteral("xcb")) {
        return nullptr;
    }

    const auto *const x11Application = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
    return x11Application == nullptr ? nullptr : x11Application->display();
}

bool x11CompositingManagerRunning(Display *const display) {
    auto selectionName = QByteArrayLiteral("_NET_WM_CM_S");
    selectionName.append(QByteArray::number(DefaultScreen(display)));
    const auto selectionAtom = XInternAtom(display, selectionName.constData(), True);
    return selectionAtom != None && XGetSelectionOwner(display, selectionAtom) != None;
}

    auto *const display = x11Application->display();
    if (display == nullptr) {
        return false;
    }

    int32_t eventBase = 0;
    if (int32_t errorBase = 0; !XShapeQueryExtension(display, &eventBase, &errorBase)) {
        return true;
    }

    if (!customFrameEnabled) {
        XShapeCombineMask(display, window->winId(), ShapeInput, 0, 0, None, ShapeSet);
        XFlush(display);
        return true;
    }

    XRectangle rectangle{
            static_cast<short>(inputRect.x()),
            static_cast<short>(inputRect.y()),
            static_cast<unsigned short>(inputRect.width()),
            static_cast<unsigned short>(inputRect.height()),
    };
    XShapeCombineRectangles(display, window->winId(), ShapeInput, 0, 0, &rectangle, 1, ShapeSet, Unsorted);
    XFlush(display);
    return true;
}
#endif

void applyInputRegion(QWindow *const window, const QRect &inputRect, const bool customFrameEnabled) {
#if QT_CONFIG(xcb)
    if (updateX11InputRegion(window, inputRect, customFrameEnabled)) {
        return;
    }
#endif

    window->setMask(customFrameEnabled ? QRegion(inputRect) : QRegion());
}

} // namespace

WindowDecorationController::WindowDecorationController(QObject *const parent) :
    QObject(parent) {}

void WindowDecorationController::updateInputRegion(QWindow *const window, const bool customFrameEnabled, const qreal frameMargin,
                                                   const qreal resizeHandleThickness) {
    if (window == nullptr) {
        return;
    }

    const auto interactiveMargin = qMax<int32_t>(0, qRound(frameMargin - resizeHandleThickness));
    const auto interactiveWidth = qMax<int32_t>(0, window->width() - 2 * interactiveMargin);
    const auto interactiveHeight = qMax<int32_t>(0, window->height() - 2 * interactiveMargin);
    const QRect inputRect{interactiveMargin, interactiveMargin, interactiveWidth, interactiveHeight};
    applyInputRegion(window, inputRect, customFrameEnabled);
}

} // namespace KDC
