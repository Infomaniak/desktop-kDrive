/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#include <QString>

/**
 * Local settings of the Linux v4 client.
 *
 * Role: hold the few values the client needs before the server parameters are loaded, or that the server does not keep:
 * the cached Sentry consent and the Matomo visitor id. Every other setting belongs to the server parameters.
 */
namespace KDC::ClientSettings {

/**
 * INI file of the client settings, `kdrive-client.ini` in the application directory (`~/.config/kDrive`), next to the
 * client lock file. Empty when that directory cannot be created; a `QSettings` on it then reads nothing and writes nothing.
 */
[[nodiscard]] QString filePath();

} // namespace KDC::ClientSettings
