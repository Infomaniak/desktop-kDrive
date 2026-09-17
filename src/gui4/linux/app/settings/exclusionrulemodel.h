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

#include "libcommon/data/exclusiontemplate.h"

#include <QAbstractListModel>
#include <QSet>

#include <optional>
#include <vector>

namespace KDC {

/** QML list adapter for one confirmed exclusion-template list and its local selection state. */
class ExclusionRuleModel final : public QAbstractListModel {
        Q_OBJECT
        Q_PROPERTY(qint32 count READ count NOTIFY countChanged)
        Q_PROPERTY(qint32 selectedCount READ selectedCount NOTIFY selectionChanged)

    public:
        enum Role {
            PatternRole = Qt::UserRole + 1,
            NotificationEnabledRole,
            SelectedRole,
        };
        Q_ENUM(Role)

        explicit ExclusionRuleModel(bool selectable, QObject *parent = nullptr);

        [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
        [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

        [[nodiscard]] qint32 count() const { return static_cast<qint32>(_rows.size()); }
        [[nodiscard]] qint32 selectedCount() const;
        [[nodiscard]] std::optional<QString> patternAt(qint32 row) const;
        [[nodiscard]] QSet<QString> selectedPatternKeys() const;

        void setRules(const std::vector<ExclusionTemplate> &templates, bool preserveSelection);
        void setSelected(qint32 row, bool selected);
        void selectAll();
        void clearSelection();

    signals:
        void countChanged();
        void selectionChanged();

    private:
        struct Row {
                QString pattern;
                bool notificationEnabled{false};
                bool selected{false};
        };

        static QString normalizedKey(const QString &pattern);

        bool _selectable{false};
        std::vector<Row> _rows;
};

} // namespace KDC
