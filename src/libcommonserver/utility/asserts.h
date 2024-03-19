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

#include <log4cplus/loggingmacros.h>

#define ASSERT_CAT(A, B) A##B
#define ASSERT_SELECT(NAME, NUM) ASSERT_CAT(NAME##_, NUM)
#define ASSERT_GET_COUNT(_1, _2, _3, COUNT, ...) COUNT
#define ASSERT_VA_SIZE(...) ASSERT_GET_COUNT(__VA_ARGS__, 3, 2, 1, 0)

#define ASSERT_OVERLOAD(NAME, ...)                   \
    ASSERT_SELECT(NAME, ASSERT_VA_SIZE(__VA_ARGS__)) \
    (__VA_ARGS__)

#define ASSERT(...) ASSERT_OVERLOAD(ASSERT, __VA_ARGS__)
#define ASSERT_1(cond)                                                                                    \
    if (!(cond)) {                                                                                        \
        LOG_FATAL(_logger, "ENFORCE: \"" << #cond << "\" in file " << __FILE__ << ", line " << __LINE__); \
    } else {                                                                                              \
    }
#define ASSERT_2(cond, message)                                                                                                 \
    if (!(cond)) {                                                                                                              \
        LOG_FATAL(_logger,                                                                                                      \
                  "ENFORCE: \"" << #cond << "\" in file " << __FILE__ << ", line " << __LINE__ << "with message: " << message); \
    } else {                                                                                                                    \
    }
