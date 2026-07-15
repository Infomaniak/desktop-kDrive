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

#include <array>
#include <cstdint>

#if QT_CONFIG(xcb)
#include <X11/Xlib.h>
#include <X11/extensions/shape.h>
#include <xcb/xcb.h>
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
using X11ExtensionCode = std::int32_t;
using X11RectangleCoordinate = std::int16_t;
using X11RectangleDimension = std::uint16_t;

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
 * Returns the xcb connection only when the application is currently running through Qt's XCB platform plugin.
 *
 * Mirrors x11Display() for the lower-level xcb API. It backs the _GTK_FRAME_EXTENTS publication, which uses
 * xcb_change_property to store a genuine 32-bit CARDINAL array (see updateX11FrameExtents).
 */
xcb_connection_t *x11Connection() {
    if (QGuiApplication::platformName() != QStringLiteral("xcb")) {
        return nullptr;
    }

    const auto *const x11Application = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
    return x11Application == nullptr ? nullptr : x11Application->connection();
}

/**
 * Returns whether the connected X server advertises the requested extension.
 */
bool x11ExtensionAvailable(Display *const display, const QByteArray &extensionName) {
    // XQueryExtension requires int pointers for its three output parameters.
    X11ExtensionCode opcode = 0;
    X11ExtensionCode eventBase = 0;
    X11ExtensionCode errorBase = 0;
    return XQueryExtension(display, extensionName.constData(), &opcode, &eventBase, &errorBase) != False;
}

/**
 * Flushes the Xlib output buffer so queued requests reach the server promptly.
 *
 * XFlush's int return is not a documented success status and is intentionally ignored: whether a native request took
 * effect cannot be known without a synchronous round-trip (XSync) plus the asynchronous error handler, which this
 * controller does not use. Callers therefore report success from having issued the request, not from this flush.
 */
void flushX11(Display *const display) {
    (void) XFlush(display);
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
    const QByteArray selectionName = QByteArrayLiteral("_NET_WM_CM_S") + QByteArray::number(DefaultScreen(display));
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

    X11ExtensionCode eventBase = 0;
    if (X11ExtensionCode errorBase = 0; !XShapeQueryExtension(display, &eventBase, &errorBase)) {
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
        flushX11(display);
        return true;
    }

    const auto nativeInputRect = toNativePixels(inputRect, window->devicePixelRatio());
    XRectangle rectangle{
            .x = static_cast<X11RectangleCoordinate>(nativeInputRect.x()),
            .y = static_cast<X11RectangleCoordinate>(nativeInputRect.y()),
            .width = static_cast<X11RectangleDimension>(nativeInputRect.width()),
            .height = static_cast<X11RectangleDimension>(nativeInputRect.height()),
    };
    XShapeCombineRectangles(display, windowId, ShapeInput, 0, 0, &rectangle, 1, ShapeSet, Unsorted);
    flushX11(display);
    return true;
}

/**
 * Publishes the invisible decoration margins used by X11 window managers for placement, snapping, and maximization.
 *
 * _GTK_FRAME_EXTENTS contains the left, right, top, and bottom margins, in native pixels, between the native window
 * bounds and the visible application frame. A zero margin tells the window manager that the complete native window is
 * visible, which is the correct state when the custom frame is disabled or maximized.
 *
 * The property is written through xcb rather than Xlib on purpose: xcb stores a format-32 property as genuine 32-bit
 * values, so std::uint32_t is exactly the expected element type. Xlib's XChangeProperty would instead require an array
 * of `long` (64-bit on LP64) for the same format. The atom itself is still interned through Xlib, which caches it
 * client-side and therefore avoids a server round-trip on every decoration update.
 */
void updateX11FrameExtents(const QWindow *const window, const qreal frameMargin) {
    auto *const display = x11Display();
    auto *const connection = x11Connection();
    if (display == nullptr || connection == nullptr) {
        return;
    }

    const auto windowId = static_cast<xcb_window_t>(window->winId());
    if (windowId == 0) {
        return;
    }

    const auto nativeMargin = static_cast<std::uint32_t>(qMax(0, qRound(frameMargin * window->devicePixelRatio())));
    // The shadow margin is uniform, so the four {left, right, top, bottom} extents all carry nativeMargin.
    const std::array<std::uint32_t, 4> frameExtents{nativeMargin, nativeMargin, nativeMargin, nativeMargin};
    // only_if_exists = False makes XInternAtom create the atom when missing, so it always returns a usable value
    // (unlike the True passed for the compositor-selection lookup, which only probes for an already-existing atom).
    const auto frameExtentsAtom = static_cast<xcb_atom_t>(XInternAtom(display, "_GTK_FRAME_EXTENTS", False));

    // xcb_change_property (re)sets _GTK_FRAME_EXTENTS on the native window to our CARDINAL[4]
    // {left, right, top, bottom} array. The window manager treats this margin as decoration/shadow area
    // outside the "real" window, excluding it from snapping, tiling and maximize geometry.
    // The format argument is the value bit-width, fixed at 32 because _GTK_FRAME_EXTENTS is CARDINAL/32
    // (EWMH). The following count is a number of elements (4), not a size in bytes.
    constexpr std::uint8_t frameExtentsFormatBits = 32;
    (void) xcb_change_property(connection, XCB_PROP_MODE_REPLACE, windowId, frameExtentsAtom, XCB_ATOM_CARDINAL,
                               frameExtentsFormatBits, frameExtents.size(), frameExtents.data());
    (void) xcb_flush(connection);
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
void applyFrameExtents(const QWindow *const window, const bool customFrameEnabled, const qreal frameMargin) {
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
    const auto interactiveWidth = qMax<int32_t>(0, window->width() - (2 * interactiveMargin));
    const auto interactiveHeight = qMax<int32_t>(0, window->height() - (2 * interactiveMargin));
    const QRect inputRect{interactiveMargin, interactiveMargin, interactiveWidth, interactiveHeight};
    applyInputRegion(window, inputRect, customFrameEnabled);
    applyFrameExtents(window, customFrameEnabled, frameMargin);
}

} // namespace KDC
