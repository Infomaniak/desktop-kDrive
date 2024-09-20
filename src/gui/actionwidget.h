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

#pragma once

#include "clickablewidget.h"

#include <QColor>
#include <QLabel>
#include <QSize>

namespace KDC {

class ActionWidget : public ClickableWidget {
        Q_OBJECT

        Q_PROPERTY(QColor background_color READ backgroundColor WRITE setBackgroundColor)
        Q_PROPERTY(QColor left_icon_color READ leftIconColor WRITE setLeftIconColor)
        Q_PROPERTY(QSize left_icon_size READ leftIconSize WRITE setLeftIconSize)
        Q_PROPERTY(QColor action_icon_color READ actionIconColor WRITE setActionIconColor)
        Q_PROPERTY(QSize action_icon_size READ actionIconSize WRITE setActionIconSize)

    public:
        explicit ActionWidget(const QString &path, const QString &text, QWidget *parent = nullptr);
        explicit ActionWidget(const QString &path, QWidget *parent = nullptr);

        void setText(const QString &text);

    signals:
        void leftIconColorChanged();
        void leftIconSizeChanged();
        void actionIconColorChanged();
        void actionIconSizeChanged();

    private:
        QString _leftIconPath;
        QString _text;
        QColor _backgroundColor;
        QColor _leftIconColor;
        QSize _leftIconSize;
        QColor _actionIconColor;
        QSize _actionIconSize;
        QLabel *_leftIconLabel;
        QLabel *_actionIconLabel;
        QLabel *_leftTextLabel;

        inline QColor backgroundColor() const { return _backgroundColor; }
        inline void setBackgroundColor(QColor color) { _backgroundColor = color; }

        inline QColor leftIconColor() const { return _leftIconColor; }
        inline void setLeftIconColor(QColor color) {
            _leftIconColor = color;
            emit leftIconColorChanged();
        }

        inline QSize leftIconSize() const { return _leftIconSize; }
        inline void setLeftIconSize(QSize size) {
            _leftIconSize = size;
            emit leftIconSizeChanged();
        }

        inline QColor actionIconColor() const { return _actionIconColor; }
        inline void setActionIconColor(const QColor &color) {
            _actionIconColor = color;
            emit actionIconColorChanged();
        }

        inline QSize actionIconSize() const { return _actionIconSize; }
        inline void setActionIconSize(const QSize &size) {
            _actionIconSize = size;
            emit actionIconSizeChanged();
        }

        void paintEvent(QPaintEvent *event) override;

        void setLeftIcon();
        void setActionIcon();

    private slots:
        void onLeftIconSizeChanged();
        void onLeftIconColorChanged();
        void onActionIconColorChanged();
        void onActionIconSizeChanged();
};

} // namespace KDC
