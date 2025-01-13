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

#include "uploadsessiontoken.h"

namespace KDC {

UploadSessionToken::UploadSessionToken() : _dbId(0), _token(std::string()) {}

UploadSessionToken::UploadSessionToken(const std::string &token) : _dbId(0), _token(token) {}

UploadSessionToken::UploadSessionToken(int64_t dbId, const std::string &token) : _dbId(dbId), _token(token) {}

} // namespace KDC
