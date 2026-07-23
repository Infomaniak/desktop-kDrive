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

#include "app/navigation/approuter.h"

#include "app/appconstants.h"

#include <QDesktopServices>
#include <QMetaEnum>

namespace KDC {

namespace {
Q_LOGGING_CATEGORY(lcAppRouter, "gui.v4.approuter", QtInfoMsg)
} // namespace

AppRouter::AppRouter(QObject *const parent) :
    QObject(parent) {}

void AppRouter::showMainWindow() {
    setMainWindowActive(true);
}

void AppRouter::hideMainWindow() {
    setMainWindowActive(false);
}

void AppRouter::showHome() {
    qCInfo(lcAppRouter) << "Main tab navigation requested | tab: Home";
    setCurrentMainTab(MainTab::Home);
}

void AppRouter::showActivities() {
    qCInfo(lcAppRouter) << "Main tab navigation requested | tab: Activities";
    setCurrentMainTab(MainTab::Activities);
}

void AppRouter::showStorage() {
    qCInfo(lcAppRouter) << "Main tab navigation requested | tab: Storage";
    setCurrentMainTab(MainTab::Storage);
}

void AppRouter::showBlockingError() {
    qCInfo(lcAppRouter) << "Main tab navigation requested | tab: BlockingError";
    setCurrentMainTab(MainTab::BlockingError);
}

void AppRouter::navigateToMainTab(const int32_t tabIndex) {
    switch (tabIndex) {
        case static_cast<int32_t>(MainTab::Home):
            setCurrentMainTab(MainTab::Home);
            return;
        case static_cast<int32_t>(MainTab::Activities):
            setCurrentMainTab(MainTab::Activities);
            return;
        case static_cast<int32_t>(MainTab::Storage):
            setCurrentMainTab(MainTab::Storage);
            return;
        case static_cast<int32_t>(MainTab::BlockingError):
            setCurrentMainTab(MainTab::BlockingError);
            return;
        default:
            qCWarning(lcAppRouter) << "Invalid main tab navigation ignored | index:" << tabIndex;
            return;
    }
}

void AppRouter::openSupport() {
    const auto url = AppConstants::Support::helpUri();
    qCInfo(lcAppRouter) << "Opening support URL:" << url;
    if (!QDesktopServices::openUrl(url)) {
        qCWarning(lcAppRouter) << "Failed to open support URL:" << url;
    }
}

void AppRouter::requestPauseCurrentSync() {
    qCInfo(lcAppRouter) << "Pause current sync requested from main toolbar | not wired yet";
}

void AppRouter::requestSearch() {
    qCInfo(lcAppRouter) << "Search requested from main toolbar | not wired yet";
}

void AppRouter::setMainWindowActive(const bool active) {
    if (_mainWindowActive == active) {
        return;
    }

    _mainWindowActive = active;
    qCInfo(lcAppRouter) << "Main window route active changed | active:" << active;
    emit mainWindowActiveChanged();
}

void AppRouter::setCurrentMainTab(const MainTab tab) {
    if (_currentMainTab == tab) {
        return;
    }

    const auto metaEnum = QMetaEnum::fromType<MainTab>();
    qCInfo(lcAppRouter) << "Main tab changed |" << metaEnum.valueToKey(static_cast<int32_t>(_currentMainTab)) << "=>"
                        << metaEnum.valueToKey(static_cast<int32_t>(tab));
    _currentMainTab = tab;
    emit currentMainTabChanged();
}

} // namespace KDC
