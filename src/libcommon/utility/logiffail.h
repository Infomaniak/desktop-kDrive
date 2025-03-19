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

#include "coveragemacros.h"
#include "filename.h"

#include <cassert>
#include <log4cplus/loggingmacros.h>


#define CAT(A, B) A##B
#define SELECT(NAME, NUM) CAT(NAME##_, NUM)
#define GET_COUNT(_1, _2, _3, COUNT, ...) COUNT
#define EXPAND(...) __VA_ARGS__
#define VA_SIZE(...) EXPAND(GET_COUNT(__VA_ARGS__, 3, 2, 1, 0))

#define OVERLOAD_WITH_ARGS_SIZE(NAME, ...) \
    SELECT(NAME, VA_SIZE(__VA_ARGS__))     \
    (__VA_ARGS__)

// Log failure message if 'cond' is false. Aborts execution in DEBUG only.
#define LOG_IF_FAIL(...) OVERLOAD_WITH_ARGS_SIZE(LOG_IF_FAIL, __VA_ARGS__)
#define LOG_IF_FAIL_1(cond)                                                                                             \
    COVERAGE_OFF                                                                                                        \
    if (!(cond)) {                                                                                                      \
        LOG_FATAL(_logger, "Condition failure: \"" << #cond << "\" in file " << __FILENAME__ << ", line " << __LINE__); \
        assert(cond);                                                                                                   \
    }                                                                                                                   \
    COVERAGE_ON

#define LOG_IF_FAIL_2(logger, cond)                                                                                    \
    COVERAGE_OFF                                                                                                       \
    if (!(cond)) {                                                                                                     \
        LOG_FATAL(logger, "Condition failure: \"" << #cond << "\" in file " << __FILENAME__ << ", line " << __LINE__); \
        assert(cond);                                                                                                  \
    }                                                                                                                  \
    COVERAGE_ON

// Log failure message if 'cond' is false. Aborts execution in DEBUG only.
#define LOG_MSG_IF_FAIL(...) OVERLOAD_WITH_ARGS_SIZE(LOG_MSG_IF_FAIL, __VA_ARGS__)
#define LOG_MSG_IF_FAIL_2(cond, message)                                                                              \
    COVERAGE_OFF                                                                                                      \
    if (!(cond)) {                                                                                                    \
        LOG_FATAL(_logger, "Condition failure: \"" << #cond << "\" in file " << __FILENAME__ << ", line " << __LINE__ \
                                                   << "with message: " << message);                                   \
        assert(cond);                                                                                                 \
    }                                                                                                                 \
    COVERAGE_ON

#define LOG_MSG_IF_FAIL_3(logger, cond, message)                                                                     \
    COVERAGE_OFF                                                                                                     \
    if (!(cond)) {                                                                                                   \
        LOG_FATAL(logger, "Condition failure: \"" << #cond << "\" in file " << __FILENAME__ << ", line " << __LINE__ \
                                                  << "with message: " << message);                                   \
        assert(cond);                                                                                                \
    }                                                                                                                \
    COVERAGE_ON
