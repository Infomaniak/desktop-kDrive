/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#include <QCollator>
#include <QLocale>
#include <QObject>
#include <QString>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

namespace KDC {

// Dedicated namespace so that Q_NAMESPACE does not cover the whole KDC namespace: QML reads these values as
// `RemoteFolders.CommonDocuments`, ... (registered in AppClientLinux::setupQmlEngine).
namespace RemoteFolders {
Q_NAMESPACE

/** Kind of a remote folder. The order of the values is the display order of siblings: special folders come first. */
enum class FolderKind : uint8_t {
    CommonDocuments = 0,
    Shared,
    Regular,
};
Q_ENUM_NS(FolderKind)

/**
 * Kind of the folder at the given drive path, as the server returns it ("/Common documents").
 *
 * Only the folders at the root of the drive are special, so a tree rooted in a sub-folder has none.
 */
[[nodiscard]] FolderKind folderKind(const QString &drivePath);

/**
 * Display order of sibling folders in the remote folder trees: special folders first, then the names in the system
 * locale, with numbers compared by value.
 */
class SiblingOrder {
    public:
        SiblingOrder();

        [[nodiscard]] bool lessThan(FolderKind lhsKind, const QString &lhsName, FolderKind rhsKind, const QString &rhsName) const;

    private:
        QCollator _collator{QLocale::system()};
};

/** Sorts sibling tree nodes, which have `kind` and `name` members, in display order. */
template<typename Node>
void sortSiblings(std::vector<std::unique_ptr<Node>> &nodes) {
    const SiblingOrder siblingOrder;
    (void) std::ranges::sort(nodes, [&siblingOrder](const std::unique_ptr<Node> &lhs, const std::unique_ptr<Node> &rhs) {
        return siblingOrder.lessThan(lhs->kind, lhs->name, rhs->kind, rhs->name);
    });
}

} // namespace RemoteFolders

using RemoteFolders::FolderKind;

} // namespace KDC
