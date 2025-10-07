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

#include "extjobmanager.h"

namespace KDC {
ExtJobManager::ExtJobManager() :
    JobManager() {}

std::shared_ptr<ExtJobManager> ExtJobManagerSingleton::_instance = nullptr;

std::shared_ptr<ExtJobManager> ExtJobManagerSingleton::instance() noexcept {
    if (_instance == nullptr) {
        try {
            _instance = std::make_shared<ExtJobManager>();
        } catch (...) {
            return nullptr;
        }
    }

    return _instance;
}

void ExtJobManagerSingleton::clear() {
    if (_instance) {
        _instance->clear();
        _instance.reset();
    }
}

} // namespace KDC
