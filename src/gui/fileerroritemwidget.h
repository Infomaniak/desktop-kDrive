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

#include "genericerroritemwidget.h"

#include "synchronizeditem.h"
#include "customwordwraplabel.h"
#include "libcommon/info/driveinfo.h"

#include <QLabel>
#include <QWidget>

namespace KDC {

class FileErrorItemWidget : public GenericErrorItemWidget {
        Q_OBJECT

        Q_PROPERTY(QColor background_color READ backgroundColor WRITE setBackgroundColor)

    public:
        explicit FileErrorItemWidget(const SynchronizedItem &item, const DriveInfo &driveInfo, QWidget *parent = nullptr);

        QSize sizeHint() const override;

    signals:
        void openFolder(const QString &filePath);

    private:
        const SynchronizedItem _item;
        QLabel *_fileNameLabel;

        inline QColor backgroundColor() const { return _backgroundColor; }
        inline void setBackgroundColor(const QColor &value) { _backgroundColor = value; }

    private slots:
        void onLinkActivated(const QString &link);
};

} // namespace KDC
