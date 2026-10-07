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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "app/mainwindow/searchresultmodel.h"
#include "app/fileformat.h"

#include <QDateTime>
#include <QLocale>
#include <QStringList>

#include <iterator>

using namespace Qt::StringLiterals;

namespace KDC {

namespace {

// Name of the folder containing the result, empty at the root of the drive.
QString parentFolderName(const SyncPath &path) {
    const SyncPath parent = path.parent_path();
    if (parent.empty() || parent == SyncPath{"."}) {
        return {};
    }

    return Path2QStr(parent.filename());
}

// "folder - date time - size". Missing parts are omitted.
QString subtitleText(const SearchInfo &info) {
    QStringList parts;

    if (const QString folder = parentFolderName(info.path()); !folder.isEmpty()) {
        parts << folder;
    }

    const QDateTime modifiedTime = QDateTime::fromSecsSinceEpoch(info.lastModifiedTime()).toLocalTime();
    parts << QLocale().toString(modifiedTime, QLocale::ShortFormat);

    if (const QString size = formatFileSize(info.type(), info.size()); !size.isEmpty()) {
        parts << size;
    }

    return parts.join(u" - "_s);
}

} // namespace

SearchResultModel::SearchResultModel(QObject *const parent) :
    QAbstractListModel(parent) {}

int SearchResultModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }

    return static_cast<int>(_rows.size());
}

QVariant SearchResultModel::data(const QModelIndex &index, const int role) const {
    if (!index.isValid() || index.row() < 0 || static_cast<size_t>(index.row()) >= _rows.size()) {
        return {};
    }

    const Row &row = _rows[static_cast<size_t>(index.row())];
    switch (role) {
        case NameRole:
            return SyncName2QStr(row.info.name());
        case FileIconNameRole:
            return row.fileIconName;
        case SubtitleTextRole:
            return subtitleText(row.info);
        case IsDirectoryRole:
            return row.info.type() == NodeType::Directory;
        case AvailableLocallyRole:
            return row.info.isAvailableLocally();
        default:
            return {};
    }
}

QHash<int, QByteArray> SearchResultModel::roleNames() const {
    return {
            {NameRole, "name"},
            {FileIconNameRole, "fileIconName"},
            {SubtitleTextRole, "subtitleText"},
            {IsDirectoryRole, "isDirectory"},
            {AvailableLocallyRole, "availableLocally"},
    };
}

int32_t SearchResultModel::replace(const std::vector<SearchInfo> &results) {
    beginResetModel();

    _rows.clear();
    _nodeIds.clear();
    _fileIconResolver.clear();

    for (const auto &info: results) {
        if (_nodeIds.insert(info.id()).second) {
            _rows.push_back(makeRow(info));
        }
    }

    endResetModel();

    return static_cast<int32_t>(_rows.size());
}

int32_t SearchResultModel::appendPage(const std::vector<SearchInfo> &results) {
    std::vector<Row> newRows;
    for (const auto &info: results) {
        if (_nodeIds.insert(info.id()).second) {
            newRows.push_back(makeRow(info));
        }
    }

    if (newRows.empty()) {
        return 0;
    }

    const auto firstRow = static_cast<int>(_rows.size());
    beginInsertRows(QModelIndex(), firstRow, firstRow + static_cast<int>(newRows.size()) - 1);
    _rows.insert(_rows.end(), std::make_move_iterator(newRows.begin()), std::make_move_iterator(newRows.end()));
    endInsertRows();

    return static_cast<int32_t>(newRows.size());
}

void SearchResultModel::clear() {
    if (_rows.empty()) {
        return;
    }

    beginResetModel();
    _rows.clear();
    _nodeIds.clear();
    endResetModel();
}

std::optional<SearchInfo> SearchResultModel::result(const int row) const {
    if (row < 0 || static_cast<size_t>(row) >= _rows.size()) {
        return std::nullopt;
    }

    return _rows[static_cast<size_t>(row)].info;
}

void SearchResultModel::retranslate() {
    if (_rows.empty()) {
        return;
    }

    emit dataChanged(index(0), index(static_cast<int>(_rows.size()) - 1), {SubtitleTextRole});
}

SearchResultModel::Row SearchResultModel::makeRow(const SearchInfo &info) const {
    return Row{info, _fileIconResolver.iconName(SyncName2QStr(info.name()), info.type())};
}

} // namespace KDC
