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

#if defined(QT_FORCE_ASSERTS) || !defined(QT_NO_DEBUG)
#define KD_ASSERT_MSG qFatal
#else
#define KD_ASSERT_MSG qCritical
#endif


// Default assert: If the condition is false in debug builds, terminate.
//
// Prints a message on failure, even in release builds.
#define QLOG_IF_FAIL(...) KD_OVERLOAD_WITH_ARGS_SIZE(ASSERT, __VA_ARGS__)
#define ASSERT_1(cond)                                                                      \
    KD_COVERAGE_OFF                                                                         \
    if (!(cond)) {                                                                          \
        KD_ASSERT_MSG("ASSERT: \"%s\" in file %s, line %d", #cond, __FILENAME__, __LINE__); \
    }                                                                                       \
    KD_COVERAGE_ON

#define ASSERT_2(cond, message)                                                                                       \
    KD_COVERAGE_OFF                                                                                                   \
    if (!(cond)) {                                                                                                    \
        KD_ASSERT_MSG("ASSERT: \"%s\" in file %s, line %d with message: %s", #cond, __FILENAME__, __LINE__, message); \
    }                                                                                                                 \
    KD_COVERAGE_ON                                                                                                    \
                                                                                                                      \
// Enforce condition to be true, even in release builds.
//
// Prints 'message' and aborts execution if 'cond' is false.
#define ENFORCE(...) KD_OVERLOAD_WITH_ARGS_SIZE(ENFORCE, __VA_ARGS__)
#define ENFORCE_1(cond)                                                               \
    KD_COVERAGE_OFF                                                                   \
    if (!(cond)) {                                                                    \
        qFatal("ENFORCE: \"%s\" in file %s, line %d", #cond, __FILENAME__, __LINE__); \
    };                                                                                \
    KD_COVERAGE_ON

#define ENFORCE_2(cond, message)                                                                                \
    KD_COVERAGE_OFF                                                                                             \
    if (!(cond)) {                                                                                              \
        qFatal("ENFORCE: \"%s\" in file %s, line %d with message: %s", #cond, __FILENAME__, __LINE__, message); \
    };                                                                                                          \
    KD_COVERAGE_ON
