
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

#pragma once

#include <log4cplus/logger.h>
#include <thread>

namespace KDC {

/**
 * @brief Checks for new updates and manage installation.
 * @ingroup gui
 *
 * This class schedules regular update checks.
 * It also trigger automatic update and installation of the new version.
 */

class AbstractUpdater {
    public:
        static std::unique_ptr<AbstractUpdater>& instance();

    private:
        AbstractUpdater();

        static void run() noexcept;

        static std::unique_ptr<AbstractUpdater> _instance;
        log4cplus::Logger _logger;
        std::unique_ptr<std::thread> _thread;
};


}  // namespace KDC
