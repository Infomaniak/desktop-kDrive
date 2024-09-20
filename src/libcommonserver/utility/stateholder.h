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

namespace KDC {

template<typename T>
class StateHolder {
    public:
        explicit StateHolder(T *item, T newValue) : _item(item) {
            if (_item) {
                _initValue = *item;
                *_item = newValue;
            }
        }

        virtual ~StateHolder() {
            if (_item) {
                *_item = _initValue;
            }
        }

    private:
        T *_item;
        T _initValue;
};

} // namespace KDC
