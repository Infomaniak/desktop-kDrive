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

#include "commonmacros.h"
#include "coveragemacros.h"
#include "filename.h"

#include <cassert>
#include <log4cplus/loggingmacros.h>

namespace KDC {
struct LogIfFailSettings {
        static inline bool enabled = true;
};
} // namespace KDC

// Log failure message if 'cond' is false. Aborts execution in DEBUG only.
#define LOG_IF_FAIL(...) KD_OVERLOAD_WITH_ARGS_SIZE(LOG_IF_FAIL, __VA_ARGS__)
#define LOG_IF_FAIL_1(cond)                                                                                             \
    KD_COVERAGE_OFF                                                                                                     \
    if (!(cond) && LogIfFailSettings::enabled) {                                                                        \
        LOG_FATAL(_logger, "Condition failure: \"" << #cond << "\" in file " << __FILENAME__ << ", line " << __LINE__); \
        assert(cond);                                                                                                   \
    }                                                                                                                   \
    KD_COVERAGE_ON

#define LOG_IF_FAIL_2(logger, cond)                                                                                    \
    KD_COVERAGE_OFF                                                                                                    \
    if (!(cond) && LogIfFailSettings::enabled) {                                                                       \
        LOG_FATAL(logger, "Condition failure: \"" << #cond << "\" in file " << __FILENAME__ << ", line " << __LINE__); \
        assert(cond);                                                                                                  \
    }                                                                                                                  \
    KD_COVERAGE_ON

// Log failure message if 'cond' is false. Aborts execution in DEBUG only.
#define LOG_MSG_IF_FAIL(...) KD_OVERLOAD_WITH_ARGS_SIZE(LOG_MSG_IF_FAIL, __VA_ARGS__)
#define LOG_MSG_IF_FAIL_2(cond, message)                                                                              \
    KD_COVERAGE_OFF                                                                                                   \
    if (!(cond) && LogIfFailSettings::enabled) {                                                                      \
        LOG_FATAL(_logger, "Condition failure: \"" << #cond << "\" in file " << __FILENAME__ << ", line " << __LINE__ \
                                                   << "with message: " << message);                                   \
        assert(cond);                                                                                                 \
    }                                                                                                                 \
    KD_COVERAGE_ON

#define LOG_MSG_IF_FAIL_3(logger, cond, message)                                                                     \
    KD_COVERAGE_OFF                                                                                                  \
    if (!(cond) && LogIfFailSettings::enabled) {                                                                     \
        LOG_FATAL(logger, "Condition failure: \"" << #cond << "\" in file " << __FILENAME__ << ", line " << __LINE__ \
                                                  << "with message: " << message);                                   \
        assert(cond);                                                                                                \
    }                                                                                                                \
    KD_COVERAGE_ON
