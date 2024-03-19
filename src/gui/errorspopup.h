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

#include <QDialog>
#include <QPaintEvent>
#include <QPoint>

namespace KDC {

class ErrorsPopup : public QDialog {
        Q_OBJECT

        Q_PROPERTY(QColor background_color READ backgroundColor WRITE setBackgroundColor)
        Q_PROPERTY(QSize warning_icon_size READ warningIconSize WRITE setWarningIconSize)
        Q_PROPERTY(QColor warning_icon_color READ warningIconColor WRITE setWarningIconColor)
        Q_PROPERTY(QSize info_icon_size READ infoIconSize WRITE setInfoIconSize)
        Q_PROPERTY(QColor info_icon_color READ infoIconColor WRITE setInfoIconColor)
        Q_PROPERTY(QSize arrow_icon_size READ arrowIconSize WRITE setArrowIconSize)
        Q_PROPERTY(QColor arrow_icon_color READ arrowIconColor WRITE setArrowIconColor)

    public:
        struct DriveError {
                int driveDbId;
                QString driveName;
                int unresolvedErrorsCount;
                int autoresolvedErrorsCount;
        };

        explicit ErrorsPopup(const QList<DriveError> &driveErrorList, int genericErrorsCount, QPoint position,
                             QWidget *parent = nullptr);
        inline int selectedAccountId() { return _selectedAccountId; }

    signals:
        void accountSelected(int accountId);

    private:
        bool _moved;
        int _selectedAccountId;
        QPoint _position;
        QColor _backgroundColor;
        QSize _warningIconSize;
        QColor _warningIconColor;
        QSize _infoIconSize;
        QColor _infoIconColor;
        QSize _arrowIconSize;
        QColor _arrowIconColor;

        void paintEvent(QPaintEvent *event) override;

        inline QColor backgroundColor() const { return _backgroundColor; }
        inline void setBackgroundColor(const QColor &value) { _backgroundColor = value; }

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

        inline QSize infoIconSize() const { return _infoIconSize; }
        inline void setInfoIconSize(const QSize &size) {
            _infoIconSize = size;
            setInfoIcon();
        }

        inline QColor infoIconColor() const { return _infoIconColor; }
        inline void setInfoIconColor(const QColor &value) {
            _infoIconColor = value;
            setInfoIcon();
        }

        inline QSize arrowIconSize() const { return _arrowIconSize; }
        inline void setArrowIconSize(const QSize &size) {
            _arrowIconSize = size;
            setArrowIcon();
        }

        inline QColor arrowIconColor() const { return _arrowIconColor; }
        inline void setArrowIconColor(const QColor &value) {
            _arrowIconColor = value;
            setArrowIcon();
        }

        void setWarningIcon();
        void setInfoIcon();
        void setArrowIcon();

    private slots:
        void onActionButtonClicked();
};

}  // namespace KDC
