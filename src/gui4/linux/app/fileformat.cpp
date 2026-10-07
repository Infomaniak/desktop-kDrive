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

#include "app/fileformat.h"

#include <QLocale>

namespace KDC {

QString formatFileSize(const NodeType nodeType, const int64_t size) {
    if (nodeType == NodeType::Directory || size < 0) {
        return {};
    }

    const QLocale locale;
    QString formatted = locale.formattedDataSize(size, 1, QLocale::DataSizeSIFormat);

    // Drop the decimal part when it is zero, to match the macOS and Windows clients.
    const QString trailingZero = locale.decimalPoint() + locale.zeroDigit();
    if (const auto index = formatted.indexOf(trailingZero); index >= 0) {
        (void) formatted.remove(index, trailingZero.size());
    }

    return formatted;
}

} // namespace KDC
