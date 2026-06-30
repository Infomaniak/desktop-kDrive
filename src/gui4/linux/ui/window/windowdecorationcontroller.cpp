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

#include <QByteArray>
#include <QGuiApplication>
#include <QRegion>
#include <QWindow>
#include <QtGui/qguiapplication_platform.h>

#include <cstdint>

#if QT_CONFIG(xcb)
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/shape.h>
#endif

namespace KDC {

namespace {

/*
 * The frameless QML window is larger than its visible surface because it reserves transparent space for a diffuse
 * shadow. If the native input region covered that complete window, the invisible shadow would intercept clicks meant
 * for applications behind kDrive.
 *
 * The controller therefore keeps only the following area interactive:
 *
 *     transparent shadow | resize handle | visible surface | resize handle | transparent shadow
 *
 * QML describes this geometry in device-independent pixels. QWindow::setMask() accepts that coordinate system and lets
 * Qt perform the platform conversion. XShape is a lower-level native X11 API, so its rectangle must first be converted
 * with the target window's device-pixel ratio.
 *
 * The controller also publishes the transparent margin through _GTK_FRAME_EXTENTS. Window managers use that metadata
 * to align the visible surface, rather than the outer edge of the shadow, when snapping or maximizing the window.
 *
 * The X11 input-region helper returns true only when the requested native operation was actually applied. A false
 * result always means that applyInputRegion() must use the Qt fallback.
 */

#if QT_CONFIG(xcb)
/**
 * Returns the Xlib display only when the application is currently running through Qt's XCB platform plugin.
 *
 * QT_CONFIG(xcb) indicates that this Qt build supports XCB; it does not guarantee that the current process uses it.
 * A Qt build containing both XCB and Wayland support can still run with the Wayland platform plugin, in which case
 * calling Xlib would be invalid.
 */
Display *x11Display() {
    if (QGuiApplication::platformName() != QStringLiteral("xcb")) {
        return nullptr;
    }

    const auto *const x11Application = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
    return x11Application == nullptr ? nullptr : x11Application->display();
}

/**
 * Returns whether the connected X server advertises the requested extension.
 */
bool x11ExtensionAvailable(Display *const display, const QByteArray &extensionName) {
    // XQueryExtension requires int pointers for its three output parameters.
    int opcode = 0;
    int eventBase = 0;
    int errorBase = 0;
    return XQueryExtension(display, extensionName.constData(), &opcode, &eventBase, &errorBase) != False;
}

/**
 * Detects whether an X11 compositing manager owns the standard per-screen compositor selection.
 *
 * A transparent top-level X11 window needs a compositor to blend its alpha channel with the desktop. Without one, the
 * reserved shadow margin can appear black or opaque. Compositors advertise themselves by owning _NET_WM_CM_S<n>, where
 * <n> is the X11 screen number. XWayland does not need to expose this selection because its Wayland compositor already
 * provides composition; that case is detected separately through the XWAYLAND extension.
 */
bool x11CompositingManagerRunning(Display *const display) {
    auto selectionName = QByteArrayLiteral("_NET_WM_CM_S");
    selectionName.append(QByteArray::number(DefaultScreen(display)));
    const auto selectionAtom = XInternAtom(display, selectionName.constData(), True);
    return selectionAtom != None && XGetSelectionOwner(display, selectionAtom) != None;
}

/**
 * Converts a logical Qt rectangle to the native pixel coordinates expected by XShape.
 *
 * The right and bottom boundaries are scaled before rebuilding the size. This keeps the converted edges consistent
 * when the device-pixel ratio is fractional; scaling x and width independently could otherwise introduce a one-pixel
 * gap or overlap through different rounding results.
 */
QRect toNativePixels(const QRect &rect, const qreal devicePixelRatio) {
    const auto left = qRound(rect.x() * devicePixelRatio);
    const auto top = qRound(rect.y() * devicePixelRatio);
    const auto right = qRound((rect.x() + rect.width()) * devicePixelRatio);
    const auto bottom = qRound((rect.y() + rect.height()) * devicePixelRatio);
    return {left, top, qMax(0, right - left), qMax(0, bottom - top)};
}

/**
 * Applies the input region directly through the X11 Shape extension.
 *
 * Returns true only after an XShape request has been issued. Returns false when X11 is not the active platform, the
 * display or native window is unavailable, or the Shape extension is unsupported, allowing applyInputRegion() to use
 * QWindow::setMask() instead.
 */
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

/**
 * Publishes the invisible decoration margins used by X11 window managers for placement, snapping, and maximization.
 *
 * _GTK_FRAME_EXTENTS contains the left, right, top, and bottom margins between the native window bounds and the visible
 * application frame. Xlib expects CARDINAL values in native pixels. A zero margin tells the window manager that the
 * complete native window is visible, which is the correct state when the custom frame is disabled or maximized.
 */
void updateX11FrameExtents(const QWindow *const window, const qreal frameMargin) {
    auto *const display = x11Display();
    if (display == nullptr) {
        return;
    }

    const auto windowId = window->winId();
    if (windowId == 0) {
        return;
    }

    const auto nativeMargin = static_cast<unsigned long>(qMax(0, qRound(frameMargin * window->devicePixelRatio())));
    const unsigned long frameExtents[] = {nativeMargin, nativeMargin, nativeMargin, nativeMargin};
    const auto frameExtentsAtom = XInternAtom(display, "_GTK_FRAME_EXTENTS", False);
    XChangeProperty(display, windowId, frameExtentsAtom, XA_CARDINAL, 32, PropModeReplace,
                    reinterpret_cast<const unsigned char *>(frameExtents), 4);
    XFlush(display);
}
#endif

/**
 * Determines whether the custom transparent shadow is safe on the current platform.
 *
 * Native Wayland and XWayland both run through a Wayland compositor. On a native X11 server, the custom shell is
 * enabled only when an X11 compositing manager owns the standard compositor selection. The result is cached by
 * WindowDecorationController and exposed to QML as a CONSTANT property.
 */
bool detectCustomShadowSupport() {
#if QT_CONFIG(xcb)
    if (QGuiApplication::platformName() == QStringLiteral("xcb")) {
        auto *const display = x11Display();
        return display != nullptr &&
               (x11ExtensionAvailable(display, QByteArrayLiteral("XWAYLAND")) || x11CompositingManagerRunning(display));
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

/**
 * Updates the platform metadata associated with the custom window frame.
 *
 * X11 window managers consume _GTK_FRAME_EXTENTS. Qt has no public API for publishing equivalent custom margins to a
 * native Wayland compositor. On native Wayland this function is therefore intentionally a no-op: the frameless window
 * and its shadow remain enabled, but snapping aligns the complete native window, including its transparent margin.
 * Consequently, the visible surface can remain separated from the screen edge by the shadow margin.
 */
void applyFrameExtents(QWindow *const window, const bool customFrameEnabled, const qreal frameMargin) {
#if QT_CONFIG(xcb)
    updateX11FrameExtents(window, customFrameEnabled ? frameMargin : 0);
#else
    Q_UNUSED(window)
    Q_UNUSED(customFrameEnabled)
    Q_UNUSED(frameMargin)
#endif
}

} // namespace

WindowDecorationController::WindowDecorationController(QObject *const parent) :
    QObject(parent),
    _customShadowsSupported(detectCustomShadowSupport()) {}

bool WindowDecorationController::customShadowsSupported() const {
    return _customShadowsSupported;
}

void WindowDecorationController::updateWindowDecoration(QWindow *const window, const bool customFrameEnabled,
                                                        const qreal frameMargin, const qreal resizeHandleThickness) {
    if (window == nullptr) {
        return;
    }

    // Keep the resize handles interactive by excluding only the outer part of the shadow margin. A maximized window
    // passes a zero frameMargin, so clamping the subtraction restores the full-window input region.
    const auto interactiveMargin = qMax<int32_t>(0, qRound(frameMargin - resizeHandleThickness));
    const auto interactiveWidth = qMax<int32_t>(0, window->width() - 2 * interactiveMargin);
    const auto interactiveHeight = qMax<int32_t>(0, window->height() - 2 * interactiveMargin);
    const QRect inputRect{interactiveMargin, interactiveMargin, interactiveWidth, interactiveHeight};
    applyInputRegion(window, inputRect, customFrameEnabled);
    applyFrameExtents(window, customFrameEnabled, frameMargin);
}

} // namespace KDC
