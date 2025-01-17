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

#include "syncfileitem.h"
#include "progress.h"

namespace KDC {

class ProgressItem {
    public:
        inline SyncFileItem &item() { return _item; }
        inline void setItem(const SyncFileItem &newItem) { _item = newItem; }
        inline Progress &progress() { return _progress; }
        inline void setProgress(const Progress &newProgress) { _progress = newProgress; }

    private:
        SyncFileItem _item;
        Progress _progress;
};

} // namespace KDC
