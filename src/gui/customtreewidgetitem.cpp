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

#include "customtreewidgetitem.h"

namespace KDC {

CustomTreeWidgetItem::CustomTreeWidgetItem(int type) : QTreeWidgetItem(type) {}

CustomTreeWidgetItem::CustomTreeWidgetItem(const QStringList &strings, int type) : QTreeWidgetItem(strings, type) {}

CustomTreeWidgetItem::CustomTreeWidgetItem(QTreeWidget *view, int type) : QTreeWidgetItem(view, type) {}

CustomTreeWidgetItem::CustomTreeWidgetItem(QTreeWidgetItem *parent, int type) : QTreeWidgetItem(parent, type) {}

bool CustomTreeWidgetItem::operator<(const QTreeWidgetItem &other) const {
    int column = treeWidget()->sortColumn();
    if (column == 1) {
        return data(1, Qt::UserRole).toLongLong() < other.data(1, Qt::UserRole).toLongLong();
    }
    return QTreeWidgetItem::operator<(other);
}

} // namespace KDC
