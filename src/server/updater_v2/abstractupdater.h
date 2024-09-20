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

#include "utility/types.h"


#include <functional>

namespace KDC {

class AbstractNetworkJob;

class AbstractUpdater {
    public:
        AbstractUpdater() = default;
        virtual ~AbstractUpdater() = default;

        virtual void onUpdateFound(const std::string &downloadUrl) = 0;
        virtual void setQuitCallback(const std::function<void()> &quitCallback) { /* Redefined in child class if necessary */ }
};

} // namespace KDC
