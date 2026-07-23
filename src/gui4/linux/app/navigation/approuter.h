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

#pragma once

#include <QLoggingCategory>
#include <QObject>

#include <cstdint>

Q_DECLARE_LOGGING_CATEGORY(lcAppRouter)

namespace KDC {

/**
 * Minimal main-window router for Linux v4.
 *
 * Role: own whether the main-window content is active and which main tab is selected.
 * Non-role: read product cache, execute backend commands, or decide onboarding eligibility.
 */
class AppRouter final : public QObject {
        Q_OBJECT
        Q_PROPERTY(bool mainWindowActive READ mainWindowActive NOTIFY mainWindowActiveChanged)
        Q_PROPERTY(MainTab currentMainTab READ currentMainTab NOTIFY currentMainTabChanged)
        Q_PROPERTY(int32_t currentMainTabIndex READ currentMainTabIndex NOTIFY currentMainTabChanged)

    public:
        enum class MainTab : uint8_t {
            Home = 0,
            Activities,
            Storage,
            BlockingError,
        };
        Q_ENUM(MainTab)

        explicit AppRouter(QObject *parent = nullptr);

        [[nodiscard]] bool mainWindowActive() const { return _mainWindowActive; }
        [[nodiscard]] MainTab currentMainTab() const { return _currentMainTab; }
        [[nodiscard]] int32_t currentMainTabIndex() const { return static_cast<int32_t>(_currentMainTab); }

        Q_INVOKABLE void showMainWindow();
        Q_INVOKABLE void hideMainWindow();
        Q_INVOKABLE void showHome();
        Q_INVOKABLE void showActivities();
        Q_INVOKABLE void showStorage();
        Q_INVOKABLE void showBlockingError();
        Q_INVOKABLE void navigateToMainTab(int32_t tabIndex);
        Q_INVOKABLE static void openSupport();
        Q_INVOKABLE static void requestPauseCurrentSync();
        Q_INVOKABLE static void requestSearch();

    signals:
        void mainWindowActiveChanged();
        void currentMainTabChanged();

    private:
        void setMainWindowActive(bool active);
        void setCurrentMainTab(MainTab tab);

        bool _mainWindowActive{false};
        MainTab _currentMainTab{MainTab::Home};
};

} // namespace KDC
