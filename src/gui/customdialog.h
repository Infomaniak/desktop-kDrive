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

#pragma once

#include "disabledoverlay.h"

#include <QColor>
#include <QDialog>
#include <QPaintEvent>
#include <QPainter>
#include <QPoint>
#include <QPointer>
#include <QStandardItem>
#include <QVBoxLayout>

namespace KDC {

class CustomDialog : public QDialog {
        Q_OBJECT

        Q_PROPERTY(QColor background_color READ backgroundColor WRITE setBackgroundColor)
        Q_PROPERTY(QColor button_icon_color READ buttonIconColor WRITE setButtonIconColor)

    public:
        explicit CustomDialog(bool popup, QWidget *parent = nullptr);

        int exec() override;
        int exec(QPoint position);
        int execAndMoveToCenter(const QWidget *parent);
        inline QVBoxLayout *mainLayout() const { return _layout; }
        void forceRedraw();
        void setBackgroundForcedColor(const QColor &value);

        bool isResizable() const;
        void setResizable(bool newIsResizable);

    signals:
        void exit();
        void viewIconSet();

    protected:
        void drawRoundRectangle();

    private:
        enum Edge {
            None = 0,
            Left,
            TopLeft,
            Top,
            TopRight,
            Right,
            BottomRight,
            Bottom,
            BottomLeft
        };

        QColor _backgroundColor;
        QColor _buttonIconColor;
        QColor _backgroundForcedColor;
        QVBoxLayout *_layout;

        bool _isResizable = false;
        bool _resizeMode = false;
        QPointF _initCursorPos;
        QPoint _initTopLeft;
        QPoint _initBottomRight;
        Edge _initEdge = None;

        QPointer<DisabledOverlay> _disabledOverlay = nullptr;

        bool eventFilter(QObject *o, QEvent *e) override;
        void paintEvent(QPaintEvent *event) override;
        void mouseMoveEvent(QMouseEvent *event) override;
        void mousePressEvent(QMouseEvent *event) override;
        void mouseReleaseEvent(QMouseEvent *event) override;

        void mouseHover(QHoverEvent *event);
        void showDisabledOverlay(bool show);

        Edge calculateCursorPosition(int x, int y);

        inline QColor backgroundColor() const { return _backgroundColor; }
        inline void setBackgroundColor(const QColor &value) { _backgroundColor = value; }

        inline QColor buttonIconColor() const { return _buttonIconColor; }
        inline void setButtonIconColor(const QColor &value) {
            _buttonIconColor = value;
            setButtonIcon(value);
        }

        virtual void setButtonIcon(const QColor &value) { Q_UNUSED(value); }

        void drawDropShadow(); // Can cause refresh issues on Windows.

    private slots:
        void onExit();
};

} // namespace KDC
