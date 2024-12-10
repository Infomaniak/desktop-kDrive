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

#ifdef __APPLE__

static const char *extAttrsStatus = "com.infomaniak.drive.desktopclient.litesync.status";
static const char *extAttrsPinState = "com.infomaniak.drive.desktopclient.litesync.pinstate";

static const char *extAttrsStatusOnline = "O";
static const char *extAttrsStatusOffline = "F";
static const char *extAttrsStatusHydrating = "H";

static const char *extAttrsPinStateUnpinned = "U";
static const char *extAttrsPinStatePinned = "P";
static const char *extAttrsPinStateExcluded = "E";

#endif
