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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "app/navigation/activitiesnavigation.h"

#include <QLoggingCategory>
#include <QMetaEnum>

namespace KDC {

namespace {
Q_LOGGING_CATEGORY(lcActivitiesNavigation, "gui.v4.approuter", QtInfoMsg)

ActivitiesNavigation::Page parentPage(const ActivitiesNavigation::Page page) {
    using Page = ActivitiesNavigation::Page;
    switch (page) {
        case Page::List:
        case Page::Errors:
            return Page::List;
        case Page::ConflictsQuick:
            return Page::Errors;
        case Page::ConflictsList:
            return Page::ConflictsQuick;
    }
    return Page::List;
}
} // namespace

ActivitiesNavigation::ActivitiesNavigation(QObject *const parent) :
    QObject(parent) {}

void ActivitiesNavigation::show(const Page page) {
    setCurrentPage(page);
}

void ActivitiesNavigation::back() {
    setCurrentPage(parentPage(_currentPage));
}

void ActivitiesNavigation::reset() {
    setCurrentPage(Page::List);
}

void ActivitiesNavigation::setCurrentPage(const Page page) {
    if (_currentPage == page) {
        return;
    }

    const auto metaEnum = QMetaEnum::fromType<Page>();
    qCInfo(lcActivitiesNavigation) << "Activities page changed |" << metaEnum.valueToKey(static_cast<int32_t>(_currentPage))
                                   << "=>" << metaEnum.valueToKey(static_cast<int32_t>(page));
    _currentPage = page;
    emit currentPageChanged();
}

} // namespace KDC
