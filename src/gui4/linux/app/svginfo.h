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

#include <QHash>
#include <QObject>
#include <QSize>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

namespace KDC {

/** Caches intrinsic icon dimensions for QML layout independently of the rasterized image size. */
class SvgInfo final : public QObject {
        Q_OBJECT
        QML_ELEMENT
        QML_SINGLETON

    public:
        explicit SvgInfo(QObject *parent = nullptr);

        /** Returns the intrinsic size of a resource or local file, or 1x1 when it cannot be read. */
        Q_INVOKABLE QSize naturalSize(const QUrl &source);

    private:
        QHash<QUrl, QSize> _sizes;
};

} // namespace KDC
