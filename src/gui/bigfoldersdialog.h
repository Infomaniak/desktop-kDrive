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

#include "customdialog.h"
#include "libcommon/info/driveinfo.h"
#include "gui/info/syncinfoclient.h"

#include <QColor>
#include <QHash>
#include <QSize>

#include <unordered_map>

namespace KDC {

class CustomCheckBox;

class BigFoldersDialog : public CustomDialog {
        Q_OBJECT

        Q_PROPERTY(QColor folder_icon_color READ folderIconColor WRITE setFolderIconColor)
        Q_PROPERTY(QSize folder_icon_size READ folderIconSize WRITE setFolderIconSize)

    public:
        explicit BigFoldersDialog(const std::unordered_map<int, std::pair<SyncInfoClient, QSet<QString>>> &syncsUndecidedMap,
                                  const DriveInfo &driveInfo, QWidget *parent = nullptr);

        const QHash<int, QHash<const QString, bool>> &mapWhiteListedSubFolders() const;

    private slots:
        void slotCheckboxClicked();

    private:
        QHash<CustomCheckBox *, int> _mapCheckboxToFolder;
        QHash<int, QHash<const QString, bool>> _mapWhiteListedSubFolders;
        QColor _folderIconColor;
        QSize _folderIconSize;

        inline QColor folderIconColor() const { return _folderIconColor; }
        inline void setFolderIconColor(QColor color) {
            _folderIconColor = color;
            setFolderIcon();
        }

        inline QSize folderIconSize() const { return _folderIconSize; }
        inline void setFolderIconSize(QSize size) {
            _folderIconSize = size;
            setFolderIcon();
        }

        void setFolderIcon();
};

} // namespace KDC
