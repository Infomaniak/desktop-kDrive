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

// This is the number that will end up in the version window of the DLLs.
// Increment this version before committing a new build if you are today's extensions build master.
#define KDEXT_BUILD_NUM 46

#define STRINGIZE2(s) #s
#define STRINGIZE(s) STRINGIZE2(s)

#define KDEXT_VERSION 1, 0, 0, KDEXT_BUILD_NUM
#define KDEXT_VERSION_STRING STRINGIZE(KDEXT_VERSION)
