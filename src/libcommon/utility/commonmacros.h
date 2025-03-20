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

#define KD_CONCAT(A, B) A##B
#define KD_APPEND_NUM(NAME, NUM) KD_CONCAT(NAME##_, NUM)
#define KD_GET_COUNT(_1, _2, _3, _4, _5, _6, _7, COUNT, ...) COUNT
#define KD_EXPAND(...) __VA_ARGS__
#define KD_VA_SIZE(...) KD_EXPAND(KD_GET_COUNT(__VA_ARGS__, 7, 6, 5, 4, 3, 2, 1, 0))

#define KD_OVERLOAD_WITH_ARGS_SIZE(NAME, ...)    \
    KD_APPEND_NUM(NAME, KD_VA_SIZE(__VA_ARGS__)) \
    (__VA_ARGS__)
