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


#include "app/errors/inotifywatchlimit.h"

#include <QFile>
#include <QLoggingCategory>

#include <algorithm>
#include <cstdint>
#include <limits>

namespace KDC {

namespace {

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(lcINotifyWatchLimit, "gui.v4.inotifywatchlimit", QtInfoMsg)

} // namespace

// Readable without any privilege. The server watches each folder of the synchronization (`FolderWatcher_linux`).
qint64 readWatchLimit() {
    QFile file(u"/proc/sys/fs/inotify/max_user_watches"_s);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcINotifyWatchLimit) << "Cannot read the inotify watches limit | error:" << file.errorString();
        return 0;
    }
    bool ok = false;
    const auto limit = file.readAll().trimmed().toLongLong(&ok);
    return ok && limit > 0 ? limit : 0;
}

// Always above the current limit, otherwise the command would change nothing: since Linux 5.11, the default depends on
// the memory (8192 to 1048576), so a fixed value alone could be lower than the current one.
qint64 proposedWatchLimit(const qint64 currentLimit) {
    // 524288 (2^19) is one of the values used by tools that watch large trees, listed in the kernel commit that made the
    // default depend on the memory (92890123749b, "inotify: Increase default inotify.max_user_watches limit to
    // 1048576"). The limit is only a cap: each watch actually registered costs about 1 KiB of kernel memory (an
    // `inotify_inode_mark` and the inode it pins), and the server registers one per folder, so a high limit costs
    // nothing until the synchronization needs it.
    // https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=92890123749bafc317bbfacbe0a62ce08d78efb7
    constexpr qint64 minimumProposedLimit = 524288;
    // The kernel stores the limit as an `int`.
    constexpr qint64 maximumLimit = std::numeric_limits<int32_t>::max();
    return std::clamp(2 * currentLimit, minimumProposedLimit, maximumLimit);
}

// Applied alone so the value takes effect now; a higher sysctl file set by the administrator still wins at boot.
QString buildWatchLimitCommand(const qint64 limit) {
    return u"echo \"fs.inotify.max_user_watches=%1\" | sudo tee /etc/sysctl.d/90-kdrive.conf"
           u" && sudo sysctl -p /etc/sysctl.d/90-kdrive.conf"_s.arg(limit);
}

} // namespace KDC
