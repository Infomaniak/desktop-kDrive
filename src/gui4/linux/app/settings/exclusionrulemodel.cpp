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

#include "exclusionrulemodel.h"

#include <cstddef>

namespace KDC {

namespace {
QString templatePattern(const ExclusionTemplate &exclusionTemplate) {
    const auto &pattern = exclusionTemplate.templ();
    return QString::fromUtf8(pattern.data(), static_cast<qsizetype>(pattern.size()));
}
} // namespace

ExclusionRuleModel::ExclusionRuleModel(const bool selectable, QObject *const parent) :
    QAbstractListModel(parent),
    _selectable(selectable) {}

int ExclusionRuleModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(_rows.size());
}

QVariant ExclusionRuleModel::data(const QModelIndex &index, const int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }

    const auto &row = _rows[static_cast<std::size_t>(index.row())];
    switch (role) {
        case PatternRole:
        case Qt::DisplayRole:
            return row.pattern;
        case NotificationEnabledRole:
            return row.notificationEnabled;
        case SelectedRole:
            return row.selected;
        default:
            return {};
    }
}

QHash<int, QByteArray> ExclusionRuleModel::roleNames() const {
    return {
            {PatternRole, "pattern"},
            {NotificationEnabledRole, "notificationEnabled"},
            {SelectedRole, "selected"},
    };
}

qint32 ExclusionRuleModel::selectedCount() const {
    qint32 count = 0;
    for (const auto &row: _rows) {
        if (row.selected) {
            ++count;
        }
    }
    return count;
}

std::optional<QString> ExclusionRuleModel::patternAt(const qint32 row) const {
    if (row < 0 || row >= count()) {
        return std::nullopt;
    }
    return _rows[static_cast<std::size_t>(row)].pattern;
}

QSet<QString> ExclusionRuleModel::selectedPatternKeys() const {
    QSet<QString> keys;
    for (const auto &row: _rows) {
        if (row.selected) {
            (void) keys.insert(normalizedKey(row.pattern));
        }
    }
    return keys;
}

void ExclusionRuleModel::setRules(const std::vector<ExclusionTemplate> &templates, const bool preserveSelection) {
    const auto selectedKeys = preserveSelection ? selectedPatternKeys() : QSet<QString>{};
    std::vector<Row> rows;
    rows.reserve(templates.size());
    for (const auto &exclusionTemplate: templates) {
        const auto pattern = templatePattern(exclusionTemplate);
        (void) rows.emplace_back(pattern, exclusionTemplate.warning(), selectedKeys.contains(normalizedKey(pattern)));
    }

    const auto previousCount = count();
    const auto previousSelectedCount = selectedCount();
    beginResetModel();
    _rows = std::move(rows);
    endResetModel();

    if (count() != previousCount) {
        emit countChanged();
    }
    if (selectedCount() != previousSelectedCount) {
        emit selectionChanged();
    }
}

void ExclusionRuleModel::setSelected(const qint32 row, const bool selected) {
    if (!_selectable || row < 0 || row >= count()) {
        return;
    }

    auto &entry = _rows[static_cast<std::size_t>(row)];
    if (entry.selected == selected) {
        return;
    }

    entry.selected = selected;
    emit dataChanged(index(row, 0), index(row, 0), {SelectedRole});
    emit selectionChanged();
}

void ExclusionRuleModel::selectAll() {
    if (!_selectable || selectedCount() == count()) {
        return;
    }
    for (auto &row: _rows) {
        row.selected = true;
    }
    if (!_rows.empty()) {
        emit dataChanged(index(0, 0), index(count() - 1, 0), {SelectedRole});
    }
    emit selectionChanged();
}

void ExclusionRuleModel::clearSelection() {
    if (!_selectable || selectedCount() == 0) {
        return;
    }
    for (auto &row: _rows) {
        row.selected = false;
    }
    if (!_rows.empty()) {
        emit dataChanged(index(0, 0), index(count() - 1, 0), {SelectedRole});
    }
    emit selectionChanged();
}

QString ExclusionRuleModel::normalizedKey(const QString &pattern) {
    return pattern.normalized(QString::NormalizationForm_C);
}

} // namespace KDC
