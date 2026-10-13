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

#pragma once

#include "app/fileiconresolver.h"
#include "libcommon/info/searchinfo.h"
#include "libcommon/utility/types.h"

#include <QAbstractListModel>
#include <QHash>
#include <QString>

#include <cstdint>
#include <optional>
#include <vector>

namespace KDC {

/**
 * QML-facing list of the search results of the current query, page after page.
 *
 * Node ids and paths stay internal: actions go through `SearchController`, which resolves a row with `result()`.
 */
class SearchResultModel final : public QAbstractListModel {
        Q_OBJECT

    public:
        enum Role {
            NameRole = Qt::UserRole + 1,
            FileIconNameRole,
            SubtitleTextRole,
            IsDirectoryRole,
            AvailableLocallyRole,
        };
        Q_ENUM(Role)

        explicit SearchResultModel(QObject *parent = nullptr);

        [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
        [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

        // Replaces every row with the first page of a new query. Returns the number of rows added.
        int32_t replace(const std::vector<SearchInfo> &results);
        // Appends a next page, skipping the nodes that a previous page already listed. Returns the number of rows added.
        int32_t appendPage(const std::vector<SearchInfo> &results);
        void clear();

        [[nodiscard]] std::optional<SearchInfo> result(int32_t row) const;

        // Refreshes the locale-dependent texts after a language change.
        void retranslate();

    private:
        struct Row {
                SearchInfo info;
                QString fileIconName;
        };

        [[nodiscard]] Row makeRow(const SearchInfo &info) const;

        std::vector<Row> _rows;
        NodeSet _nodeIds;
        FileIconResolver _fileIconResolver;
};

} // namespace KDC
