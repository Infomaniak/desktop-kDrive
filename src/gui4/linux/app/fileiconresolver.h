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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "libcommon/utility/cstypes.h"

#include <QHash>
#include <QString>

namespace KDC {

/** Resolves a local file name to the semantic file-icon asset used by the Linux v4 UI. */
class FileIconResolver final {
    public:
        /** Returns an icon asset name such as `file-image`, `file` when no specialized icon matches, or `folder` for
         * directories, which the view renders from its own asset. */
        [[nodiscard]] QString iconName(const QString &fileName, NodeType nodeType) const;

        /** Drops the cached names. Call it when the resolved set is replaced wholesale, such as on a sync switch. */
        void clear() const;

    private:
        mutable QHash<QString, QString> _iconNamesByFileName;
};

} // namespace KDC
