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

class DisplayErrorsWidget : public ClickableWidget {
        Q_OBJECT

        Q_PROPERTY(QColor background_color READ backgroundColor WRITE setBackgroundColor)
        Q_PROPERTY(QColor warning_icon_color READ warningIconColor WRITE setWarningIconColor)
        Q_PROPERTY(QSize warning_icon_size READ warningIconSize WRITE setWarningIconSize)
        Q_PROPERTY(QColor action_icon_color READ actionIconColor WRITE setActionIconColor)
        Q_PROPERTY(QSize action_icon_size READ actionIconSize WRITE setActionIconSize)

    public:
        explicit DisplayErrorsWidget(QWidget *parent = nullptr);

    signals:
        void warningIconColorChanged();
        void warningIconSizeChanged();
        void actionIconColorChanged();
        void actionIconSizeChanged();
        void displayErrors();

    private:
        QColor _backgroundColor;
        QColor _warningIconColor;
        QSize _warningIconSize;
        QColor _actionIconColor;
        QSize _actionIconSize;
        QLabel *_warningIconLabel;
        QLabel *_actionIconLabel;

        inline QColor backgroundColor() const { return _backgroundColor; }
        inline void setBackgroundColor(QColor color) { _backgroundColor = color; }

        inline QColor warningIconColor() const { return _warningIconColor; }
        inline void setWarningIconColor(QColor color) {
            _warningIconColor = color;
            emit warningIconColorChanged();
        }

        inline QSize warningIconSize() const { return _warningIconSize; }
        inline void setWarningIconSize(QSize size) {
            _warningIconSize = size;
            emit warningIconSizeChanged();
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

        void setWarningIcon();
        void setActionIcon();

    private slots:
        void onWarningIconSizeChanged();
        void onWarningIconColorChanged();
        void onActionIconColorChanged();
        void onActionIconSizeChanged();
        void onClick();
};

}  // namespace KDC
