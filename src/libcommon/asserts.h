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

// #include <qglobal.h>

#if defined(QT_FORCE_ASSERTS) || !defined(QT_NO_DEBUG)
#define KD_ASSERT_MSG qFatal
#else
#define KD_ASSERT_MSG qCritical
#endif

// For overloading macros by argument count
// See stackoverflow.com/questions/16683146/can-macros-be-overloaded-by-number-of-arguments
#define KD_ASSERT_CAT(A, B) A##B
#define KD_ASSERT_SELECT(NAME, NUM) KD_ASSERT_CAT(NAME##_, NUM)
#define KD_ASSERT_GET_COUNT(_1, _2, _3, COUNT, ...) COUNT
#define KD_ASSERT_VA_SIZE(...) KD_ASSERT_GET_COUNT(__VA_ARGS__, 3, 2, 1, 0)

#define KD_ASSERT_OVERLOAD(NAME, ...)                      \
    KD_ASSERT_SELECT(NAME, KD_ASSERT_VA_SIZE(__VA_ARGS__)) \
    (__VA_ARGS__)

// Default assert: If the condition is false in debug builds, terminate.
//
// Prints a message on failure, even in release builds.
#define ASSERT(...) KD_ASSERT_OVERLOAD(ASSERT, __VA_ARGS__)
#define ASSERT_1(cond)                                                                  \
    if (!(cond)) {                                                                      \
        KD_ASSERT_MSG("ASSERT: \"%s\" in file %s, line %d", #cond, __FILE__, __LINE__); \
    } else {                                                                            \
    }
#define ASSERT_2(cond, message)                                                                                   \
    if (!(cond)) {                                                                                                \
        KD_ASSERT_MSG("ASSERT: \"%s\" in file %s, line %d with message: %s", #cond, __FILE__, __LINE__, message); \
    } else {                                                                                                      \
    }

// Enforce condition to be true, even in release builds.
//
// Prints 'message' and aborts execution if 'cond' is false.
#define ENFORCE(...) KD_ASSERT_OVERLOAD(ENFORCE, __VA_ARGS__)
#define ENFORCE_1(cond)                                                           \
    if (!(cond)) {                                                                \
        qFatal("ENFORCE: \"%s\" in file %s, line %d", #cond, __FILE__, __LINE__); \
    } else {                                                                      \
    }
#define ENFORCE_2(cond, message)                                                                            \
    if (!(cond)) {                                                                                          \
        qFatal("ENFORCE: \"%s\" in file %s, line %d with message: %s", #cond, __FILE__, __LINE__, message); \
    } else {                                                                                                \
    }
