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

#include <log4cplus/loggingmacros.h>

#define LOG_IF_FAIL_CAT(A, B) A##B
#define LOG_IF_FAIL_SELECT(NAME, NUM) LOG_IF_FAIL_CAT(NAME##_, NUM)
#define LOG_IF_FAIL_GET_COUNT(_1, _2, _3, COUNT, ...) COUNT
#define LOG_IF_FAIL_VA_SIZE(...) LOG_IF_FAIL_GET_COUNT(__VA_ARGS__, 3, 2, 1, 0)

#define LOG_IF_FAIL_OVERLOAD(NAME, ...)                        \
    LOG_IF_FAIL_SELECT(NAME, LOG_IF_FAIL_VA_SIZE(__VA_ARGS__)) \
    (__VA_ARGS__)

#define LOG_IF_FAIL(...) LOG_IF_FAIL_OVERLOAD(LOG_IF_FAIL, __VA_ARGS__)
#define LOG_IF_FAIL_LOGGER(logger, ...) LOG_IF_FAIL_LOG(logger, __VA_ARGS__)
#define LOG_IF_FAIL_1(cond)                                                                               \
    if (!(cond)) {                                                                                        \
        LOG_FATAL(_logger, "ENFORCE: \"" << #cond << "\" in file " << __FILE__ << ", line " << __LINE__); \
        sentry::Handler::captureMessage(sentry::Level::Error, "ENFORCE", #cond);                        \
    }

#define LOG_IF_FAIL_2(cond, message)                                                                                            \
    if (!(cond)) {                                                                                                              \
        LOG_FATAL(_logger,                                                                                                      \
                  "ENFORCE: \"" << #cond << "\" in file " << __FILE__ << ", line " << __LINE__ << "with message: " << message); \
    }

#define LOG_IF_FAIL_LOG(logger, cond)                                                                               \
    if (!(cond)) {                                                                                        \
        LOG_FATAL(logger, "ENFORCE: \"" << #cond << "\" in file " << __FILE__ << ", line " << __LINE__); \
        sentry::Handler::captureMessage(sentry::Level::Error, "ENFORCE", #cond);                          \
    }
