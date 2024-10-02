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

#include "synthesisitemdelegate.h"
#include "synchronizeditem.h"

#include <iostream>
#include <cstdlib>

#include <QApplication>
#include <QFileInfo>
#include <QColor>
#include <QMimeDatabase>
#include <QMimeType>

namespace KDC {

SynthesisItemDelegate::SynthesisItemDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

void SynthesisItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    QVariant data = index.data();
    if (data.canConvert<SynchronizedItem>()) {
        SynchronizedItem item = qvariant_cast<SynchronizedItem>(data);
        bool isSelected = option.state & QStyle::State_Selected;

        painter->save();


        painter->restore();
    } else {
        // Should not happen
        QStyledItemDelegate::paint(painter, option, index);
    }
}

} // namespace KDC
