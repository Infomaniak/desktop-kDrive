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

#include <QObject>

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
         */
        Q_INVOKABLE static void updateWindowDecoration(QWindow *window, bool customFrameEnabled, qreal frameMargin,
                                                       qreal resizeHandleThickness);

    private:
        bool _customShadowsSupported = false;
};

} // namespace KDC
