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

#include "localpaths.h"

#include <QDir>
#include <QFileInfo>

using namespace Qt::StringLiterals;

namespace KDC {

namespace {
constexpr auto homeShorthand = "~"_L1;
// Mirrors the give-up threshold of ServerRequests::findGoodPathForNewSync() in src/server/requests/serverrequests.cpp.
constexpr uint32_t maxUniqueLocalPathAttempts = 100;

// The filesystem root already ends with its separator: appending a second one would build "//" and never match.
bool containsPath(const QString &ancestor, const QString &path) {
    return path.startsWith(ancestor.endsWith(u'/') ? ancestor : ancestor + u'/');
}
} // namespace

QString displayLocalPath(const QString &localPath) {
    if (localPath.isEmpty()) return {};

    const QString cleanPath = QDir::cleanPath(localPath);
    const QString homePath = QDir::cleanPath(QDir::homePath());
    if (homePath.isEmpty() || homePath == u"/"_s) return cleanPath;
    if (cleanPath == homePath) return homeShorthand;
    if (cleanPath.startsWith(homePath + u'/')) return homeShorthand + cleanPath.mid(homePath.size());
    return cleanPath;
}

bool localPathsOverlap(const QString &lhs, const QString &rhs) {
    const QString cleanLhs = QDir::cleanPath(lhs);
    const QString cleanRhs = QDir::cleanPath(rhs);
    if (cleanLhs == cleanRhs) return true;
    return containsPath(cleanRhs, cleanLhs) || containsPath(cleanLhs, cleanRhs);
}

QString makeUniqueLocalPath(const QString &path, const std::function<bool(const QString &)> &taken) {
    const QString cleanPath = QDir::cleanPath(path);
    if (cleanPath.isEmpty() || !taken(cleanPath)) return cleanPath;

    // Existence on disk is checked here too: the server vouched for the path it proposed, not for the ones derived
    // from it, and a folder named like the candidate may well already sit next to it.
    for (uint32_t attempt = 2; attempt <= maxUniqueLocalPathAttempts; ++attempt) {
        // Same suffix as ServerRequests::findGoodPathForNewSync(): the attempt count appended to the folder name,
        // with no separator. Keep both sides in step, see the contract documented on this function.
        if (const QString candidate = cleanPath + QString::number(attempt); !taken(candidate) && !QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
    return {};
}

} // namespace KDC
