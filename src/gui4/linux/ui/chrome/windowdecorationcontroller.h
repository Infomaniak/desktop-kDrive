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

#pragma once

#include <QHash>
#include <QObject>

class QEvent;
class QWindow;

namespace KDC {

/**
 * Provides the platform integration required by the QML frameless-window shell.
 *
 * The QML window reserves transparent space around its visible surface for a custom drop shadow. This controller
 * prevents most of that transparent margin from receiving pointer events while preserving the resize handles placed
 * next to the surface. It also reports whether the current windowing environment can display transparent custom
 * shadows safely.
 */
class WindowDecorationController final : public QObject {
        Q_OBJECT
        Q_PROPERTY(bool customShadowsSupported READ customShadowsSupported CONSTANT)

    public:
        explicit WindowDecorationController(QObject *parent = nullptr);

        /**
         * Returns whether the current platform can display the transparent custom-shadow shell.
         *
         * The value is detected once when the controller is constructed and remains constant for its lifetime.
         */
        [[nodiscard]] bool customShadowsSupported() const;

        /**
         * Aligns the native window decoration metadata and input region with the visible surface and resize handles.
         *
         * QML passes logical-pixel measurements. The platform implementation performs any required conversion before
         * updating the native window.
         *
         * The request is remembered so that it can be replayed on every later exposure of @p window. See eventFilter().
         */
        Q_INVOKABLE void updateWindowDecoration(QWindow *window, bool customFrameEnabled, qreal frameMargin,
                                                qreal resizeHandleThickness);

    private:
        /**
         * Replays the last decoration request whenever a tracked window becomes exposed.
         *
         * The input region is expressed in logical pixels but consumed in surface coordinates, and the two only agree
         * once the native surface is mapped with its final scale. An application-wide scale factor makes them differ by
         * that factor until then, which leaves a painted but click-through band along the right and bottom edges.
         * Applying the region once, when QML computes it, is therefore not enough: a window that is hidden and shown
         * again never recomputes it and keeps the mismatched region for the rest of its life. Exposure is the earliest
         * point where the surface is guaranteed to be mapped, so the region is reapplied there, for every window and
         * every code path that shows one.
         */
        bool eventFilter(QObject *watched, QEvent *event) override;

        struct DecorationRequest {
                bool customFrameEnabled = false;
                qreal frameMargin = 0;
                qreal resizeHandleThickness = 0;
        };

        /** Applies @p request to @p window without touching the stored requests. */
        static void applyDecoration(QWindow *window, const DecorationRequest &request);

        bool _customShadowsSupported = false;
        QHash<const QWindow *, DecorationRequest> _decorationRequests;
};

} // namespace KDC
