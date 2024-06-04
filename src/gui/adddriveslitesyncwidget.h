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

#include "foldertreeitemwidget.h"

#include <QColor>
#include <QIcon>
#include <QLabel>
#include <QNetworkReply>
#include <QPushButton>
#include <QSize>
#include <QStringList>
#include <QTreeWidget>

namespace KDC {

class AddDriveLiteSyncWidget : public QWidget {
        Q_OBJECT

        Q_PROPERTY(QColor logo_color READ logoColor WRITE setLogoColor)

    public:
        explicit AddDriveLiteSyncWidget(QWidget *parent = nullptr);

        void setButtonIcon(const QColor &value);

        inline bool liteSync() const { return _liteSync; }

    signals:
        void terminated(bool next = true);

    private:
        QLabel *_logoTextIconLabel;
        QPushButton *_backButton;
        QPushButton *_laterButton;
        QPushButton *_yesButton;
        QColor _logoColor;
        bool _liteSync;

        inline QColor logoColor() const { return _logoColor; }
        void setLogoColor(const QColor &color);

        void initUI();

    private slots:
        void onLinkActivated(const QString &link);
        void onBackButtonTriggered(bool checked = false);
        void onLaterButtonTriggered(bool checked = false);
        void onYesButtonTriggered(bool checked = false);
};

}  // namespace KDC
