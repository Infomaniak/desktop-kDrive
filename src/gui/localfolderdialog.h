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

#include "customdialog.h"

#include <QColor>
#include <QLabel>
#include <QPushButton>
#include <QSize>

namespace KDC {

class LocalFolderDialog : public CustomDialog {
        Q_OBJECT

        Q_PROPERTY(QColor folder_icon_color READ folderIconColor WRITE setFolderIconColor)
        Q_PROPERTY(QSize folder_icon_size READ folderIconSize WRITE setFolderIconSize)
        Q_PROPERTY(QSize warning_icon_size READ warningIconSize WRITE setWarningIconSize)
        Q_PROPERTY(QColor warning_icon_color READ warningIconColor WRITE setWarningIconColor)

    public:
        explicit LocalFolderDialog(const QString &localFolderPath, QWidget *parent = nullptr);

        inline QString localFolderPath() const { return _localFolderPath; }
        inline void setSmartSync(bool smartSync) { _smartSync = smartSync; }
        inline bool folderCompatibleWithSmartSync() const { return _folderCompatibleWithSmartSync; }

    signals:
        void openFolder(const QString &filePath);

    private:
        QString _localFolderPath;
        QPushButton *_continueButton;
        QWidget *_folderSelectionWidget;
        QWidget *_folderSelectedWidget;
        QLabel *_folderIconLabel;
        QLabel *_folderNameLabel;
        QLabel *_folderPathLabel;
        QColor _folderIconColor;
        QSize _folderIconSize;
        QColor _warningIconColor;
        QSize _warningIconSize;
        QWidget *_warningWidget;
        QLabel *_warningIconLabel;
        QLabel *_warningLabel;
        bool _okToContinue;
        bool _smartSync;
        bool _folderCompatibleWithSmartSync;

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

        inline QSize warningIconSize() const { return _warningIconSize; }
        inline void setWarningIconSize(const QSize &size) {
            _warningIconSize = size;
            setWarningIcon();
        }

        inline QColor warningIconColor() const { return _warningIconColor; }
        inline void setWarningIconColor(const QColor &value) {
            _warningIconColor = value;
            setWarningIcon();
        }

        void initUI();
        void updateUI();
        void setOkToContinue(bool value);
        void selectFolder(const QString &startDirPath);
        void setFolderIcon();
        void setWarningIcon();

    private slots:
        void onExit();
        void onContinueButtonTriggered(bool checked = false);
        void onSelectFolderButtonTriggered(bool checked = false);
        void onUpdateFolderButtonTriggered(bool checked = false);
        void onLinkActivated(const QString &link);
};

}  // namespace KDC
