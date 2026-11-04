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

#include "app/svginfo.h"

#include <QImageReader>
#include <QLoggingCategory>

using namespace Qt::StringLiterals;

namespace KDC {

namespace {
Q_LOGGING_CATEGORY(lcSvgInfo, "gui.v4.svg", QtWarningMsg)
} // namespace

SvgInfo::SvgInfo(QObject *parent) :
    QObject(parent) {}

QSize SvgInfo::naturalSize(const QUrl &source) {
    if (source.isEmpty()) {
        return {1, 1};
    }
    if (const auto it = _sizes.constFind(source); it != _sizes.cend()) {
        return *it;
    }

    const QString path = source.scheme() == u"qrc"_s ? u":"_s + source.path() : source.toLocalFile();
    QImageReader reader(path);
    const QSize size = reader.size();
    if (size.isEmpty()) {
        qCWarning(lcSvgInfo) << "Cannot read SVG dimensions" << source << reader.errorString();
        return {1, 1};
    }

    (void) _sizes.insert(source, size);
    return size;
}

} // namespace KDC
