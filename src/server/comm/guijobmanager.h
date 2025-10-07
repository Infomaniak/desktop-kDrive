
/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
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

#include "libsyncengine/jobs/jobmanager.h"

namespace KDC {

class GuiJobManager : public JobManager {
    public:
        ~GuiJobManager() override = default;
        GuiJobManager();
        GuiJobManager(GuiJobManager const &) = delete;
        void operator=(GuiJobManager const &) = delete;
};

class GuiJobManagerSingleton {
    public:
        static std::shared_ptr<GuiJobManager> instance() noexcept;
        static void clear();

    private:
        static std::shared_ptr<GuiJobManager> _instance;
};

} // namespace KDC
