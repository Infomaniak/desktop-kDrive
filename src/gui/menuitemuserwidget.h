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

#include <QColor>
#include <QIcon>
#include <QLabel>
#include <QPaintEvent>
#include <QSize>
#include <QString>
#include <QWidget>

namespace KDC {

class MenuItemUserWidget : public QWidget {
        Q_OBJECT

    public:
        MenuItemUserWidget(const QString &name, const QString &email, bool currentUser = false, QWidget *parent = nullptr);

        void setLeftImage(const QImage &data);
        static QMargins getMargins();

    private:
        QString _name;
        QString _email;
        QLabel *_leftPictureLabel;
        int _leftPictureSize;
        bool _checked;
        bool _hasSubmenu;


        void initCurrentUserUI();
        void initUserUI();
        void paintEvent(QPaintEvent *paintEvent);
};

} // namespace KDC
