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

#include "utility/types.h"

#include <QLabel>
#include <QWidget>
#include <QBoxLayout>
#include <QListWidgetItem>
#include <QFileInfo>

class QHBoxLayout;

namespace KDC {

class ClientGui;

class AbstractFileItemWidget : public QWidget {
        Q_OBJECT

        Q_PROPERTY(QColor background_color READ backgroundColor WRITE setBackgroundColor)
        Q_PROPERTY(QColor logo_color READ logoColor WRITE setLogoColor)

    public:
        explicit AbstractFileItemWidget(QWidget *parent = nullptr);

        virtual void init() = 0;

        QSize sizeHint() const override;

        void setFilePath(const QString &filePath, NodeType type = NodeType::File);
        void setDriveName(const QString &driveName, const QString &localPath);
        void setPathIconColor(const QColor &color);
        void setMessage(const QString &str);

        void addCustomWidget(QWidget *w);

    protected slots:
        virtual void openFolder(const QString &path);

    private:
        inline QColor backgroundColor() const { return _backgroundColor; }
        inline void setBackgroundColor(const QColor &value) { _backgroundColor = value; }
        inline QColor logoColor() const { return _logoColor; }
        void setLogoColor(const QColor &value);
        void paintEvent(QPaintEvent *event) override;

        void setFileTypeIcon(const QString &ressourcePath);
        void setFileName(const QString &path, NodeType type = NodeType::File);
        void setPath(const QString &path);

        QColor _backgroundColor;
        QColor _logoColor;

        QHBoxLayout *_topLayout = nullptr;
        QLabel *_fileTypeIconLabel = nullptr;
        QLabel *_filenameLabel = nullptr;

        QHBoxLayout *_middleLayout = nullptr;
        QLabel *_messageLabel = nullptr;

        QHBoxLayout *_bottomLayout = nullptr;
        QLabel *_driveIconLabel = nullptr;
        QLabel *_pathLabel = nullptr;
};

} // namespace KDC
