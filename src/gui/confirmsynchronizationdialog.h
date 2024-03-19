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

#include <QLabel>
#include <QPushButton>

namespace KDC {

class ClientGui;

class ConfirmSynchronizationDialog : public CustomDialog {
        Q_OBJECT

        Q_PROPERTY(QColor arrow_icon_color READ arrowIconColor WRITE setArrowIconColor)

    public:
        explicit ConfirmSynchronizationDialog(std::shared_ptr<ClientGui> gui, int userDbId, int driveId,
                                              const QString &serverFolderNodeId, const QString &localFolderName,
                                              qint64 localFolderSize, const QString &serverFolderName, qint64 serverFolderSize,
                                              QWidget *parent = nullptr);

    private:
        std::shared_ptr<ClientGui> _gui;

        QString _localFolderName;
        qint64 _localFolderSize;
        QString _serverFolderName;
        qint64 _serverFolderSize;
        QLabel *_leftArrowIconLabel;
        QLabel *_rightArrowIconLabel;
        QLabel *_serverSizeLabel;
        QPushButton *_backButton;
        QPushButton *_continueButton;
        QColor _arrowIconColor;

        void setButtonIcon(const QColor &value) override;

        inline QColor arrowIconColor() const { return _arrowIconColor; }
        inline void setArrowIconColor(QColor color) {
            _arrowIconColor = color;
            setArrowIcon();
        }

        void initUI();
        void setArrowIcon();

    private slots:
        void onExit();
        void onBackButtonTriggered(bool checked = false);
        void onContinueButtonTriggered(bool checked = false);
        void onFolderSizeCompleted(QString nodeId, qint64 size);
};

}  // namespace KDC
