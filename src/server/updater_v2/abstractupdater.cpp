
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

#include "abstractupdater.h"

namespace KDC {

std::unique_ptr<AbstractUpdater>& AbstractUpdater::instance() {
    if (!_instance) {
        _instance = std::make_unique<AbstractUpdater>();
    }
    return _instance;
}

AbstractUpdater::AbstractUpdater() {
    _thread = std::make_unique<std::thread>(run);
}

void AbstractUpdater::run() noexcept {
    // To be implemented
}

}  // namespace KDC