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

#include "customwordwraplabel.h"

#include <QApplication>
#include <QRect>
#include <QString>
#include <QScreen>

namespace KDC {

CustomWordWrapLabel::CustomWordWrapLabel(QWidget *parent, Qt::WindowFlags f) : QLabel(parent, f), _maxWidth(0) {
    setWordWrap(true);
}

CustomWordWrapLabel::CustomWordWrapLabel(const QString &text, QWidget *parent, Qt::WindowFlags f) :
    QLabel(text, parent, f), _maxWidth(0) {
    setWordWrap(true);
}

QSize CustomWordWrapLabel::sizeHint() const {
    if (_maxWidth) {
        // Text height processing
        QStringList lines = text().split("<br>");
        QFontMetrics metrics(font());
        int height = 0;
        for (QString &line: lines) {
            QRect textRect = metrics.boundingRect(screen()->geometry(), static_cast<int>(alignment() | Qt::TextWordWrap), line);
            int nbLines = textRect.width() / _maxWidth + 1;
            height += textRect.height() * nbLines;
        }

        // Total height
        return QSize(QLabel::sizeHint().width(), height + contentsMargins().top() + contentsMargins().bottom());
    }

    return QLabel::sizeHint();
}

void CustomWordWrapLabel::setMaxWidth(int width) {
    _maxWidth = width;
    setFixedHeight(sizeHint().height());
}

} // namespace KDC
