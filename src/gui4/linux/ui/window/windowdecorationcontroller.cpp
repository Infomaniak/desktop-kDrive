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
QRect toNativePixels(const QRect &rect, const qreal devicePixelRatio) {
    const auto left = qRound(rect.x() * devicePixelRatio);
    const auto top = qRound(rect.y() * devicePixelRatio);
    const auto right = qRound((rect.x() + rect.width()) * devicePixelRatio);
    const auto bottom = qRound((rect.y() + rect.height()) * devicePixelRatio);
    return {left, top, qMax(0, right - left), qMax(0, bottom - top)};
}

bool updateX11InputRegion(const QWindow *const window, const QRect &inputRect, const bool customFrameEnabled) {
    auto *const display = x11Display();
    if (display == nullptr) {
        return false;
    }

    int32_t eventBase = 0;
    if (int32_t errorBase = 0; !XShapeQueryExtension(display, &eventBase, &errorBase)) {
        return false;
    }

    // winId() creates the native platform window when necessary. An unmapped X11 window may still have a valid XID.
    const auto windowId = window->winId();
    if (windowId == 0) {
        return false;
    }

    if (!customFrameEnabled) {
        // A null ShapeInput mask restores the default full-window input region.
        XShapeCombineMask(display, windowId, ShapeInput, 0, 0, None, ShapeSet);
        XFlush(display);
        return true;
    }

    const auto nativeInputRect = toNativePixels(inputRect, window->devicePixelRatio());
    XRectangle rectangle{
            .x = static_cast<short>(nativeInputRect.x()),
            .y = static_cast<short>(nativeInputRect.y()),
            .width = static_cast<unsigned short>(nativeInputRect.width()),
            .height = static_cast<unsigned short>(nativeInputRect.height()),
    };
    XShapeCombineRectangles(display, windowId, ShapeInput, 0, 0, &rectangle, 1, ShapeSet, Unsorted);
    XFlush(display);
    return true;
}
#endif

/**
 * Determines whether the custom transparent shadow is safe on the current platform.
 *
 * Wayland always runs through a compositor, so non-XCB platforms are accepted. On X11, the custom shell is enabled
 * only when both the native display and an active compositing manager are available. The result is cached by
 * WindowDecorationController and exposed to QML as a CONSTANT property.
 */
bool detectCustomShadowSupport() {
#if QT_CONFIG(xcb)
    if (QGuiApplication::platformName() == QStringLiteral("xcb")) {
        auto *const display = x11Display();
        return display != nullptr && x11CompositingManagerRunning(display);
    }
#endif
    return true;
}

/**
 * Applies the input region using the most appropriate available integration.
 *
 * The direct XShape path avoids clipping the rendered shadow while changing only ShapeInput. When that path cannot be
 * used, QWindow::setMask() provides the portable fallback and accepts the original logical-pixel rectangle.
 */
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
    QObject(parent),
    _customShadowsSupported(detectCustomShadowSupport()) {}

bool WindowDecorationController::customShadowsSupported() const {
    return _customShadowsSupported;
}

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
