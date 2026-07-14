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

#include "syncfolderallowedchecker.h"

#include "libcommon/utility/utility.h"
#include "libcommonserver/log/log.h"
#include "libparms/db/parmsdb.h"
#include "libparms/db/syncfolderrule.h"

#include <QDir>

namespace KDC {

ExitInfo SyncFolderAllowedChecker::check(const SyncPath &path, bool &allowed) {
    allowed = true;
    LOGW_DEBUG(Log::instance()->getLogger(), L"isSyncFolderAllowedByRules START: path=" << Utility::formatSyncPath(path));

    std::vector<SyncFolderRule> rules;
    if (!ParmsDb::instance()->selectAllSyncFolderRules(rules)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllSyncFolderRules");
        return ExitCode::DbError;
    }

    if (rules.empty()) {
        LOG_DEBUG(Log::instance()->getLogger(), "isSyncFolderAllowedByRules: no rules found, allowing path");
        sentry::Handler::captureMessage(sentry::Level::Warning, "ServerRequests::isSyncFolderAllowedByRules",
                                        "No sync rules found, allowing path");
        return ExitCode::Ok;
    }

    LOG_DEBUG(Log::instance()->getLogger(), "isSyncFolderAllowedByRules: found " << rules.size() << " rules");

    const QString candidateDir = QDir::cleanPath(Path2QStr(path)) + '/';

    const SyncFolderRule *bestMatch = nullptr;
    SyncPath bestMatchExpandedPath;
    int32_t bestDepth = -1;

    for (const auto &rule: rules) {
        LOGW_DEBUG(Log::instance()->getLogger(), L"isSyncFolderAllowedByRules: checking rule syncPath: "
                                                         << Utility::formatSyncPath(rule.syncPath()) << L" type: "
                                                         << static_cast<int>(rule.folderRuleType()));

        SyncPath expandedRulePath = expandRulePath(rule.syncPath());

        LOGW_DEBUG(Log::instance()->getLogger(),
                   L"isSyncFolderAllowedByRules: expanded rule syncPath: " << Utility::formatSyncPath(expandedRulePath));

        if (const QString ruleDir = QDir::cleanPath(Path2QStr(expandedRulePath)) + '/';
            !candidateDir.startsWith(ruleDir, Qt::CaseSensitive))
            continue;
        LOGW_DEBUG(Log::instance()->getLogger(), L"isSyncFolderAllowedByRules: rule matched");

        if (const int32_t depth = Utility::pathDepth(expandedRulePath); depth > bestDepth) {
            bestDepth = depth;
            bestMatch = &rule;
            bestMatchExpandedPath = expandedRulePath;
        }
    }

    if (bestMatch) {
        LOGW_DEBUG(Log::instance()->getLogger(),
                   L"isSyncFolderAllowedByRules: bestMatch syncPath: " << Utility::formatSyncPath(bestMatchExpandedPath));
    } else {
        allowed = false;
        LOG_DEBUG(Log::instance()->getLogger(), "isSyncFolderAllowedByRules RESULT: allowed=false (no matching rule)");
        return ExitCode::Ok;
    }


    switch (bestMatch->folderRuleType()) {
        case SyncFolderRuleType::BlackList:
            LOGW_INFO(Log::instance()->getLogger(), L"Path rejected by blacklist rule \""
                                                            << Utility::formatSyncPath(bestMatchExpandedPath) << L"\": "
                                                            << Utility::formatSyncPath(path));
            allowed = false;
            break;
        case SyncFolderRuleType::WhiteList:
            allowed = true;
            LOG_DEBUG(Log::instance()->getLogger(), "isSyncFolderAllowedByRules RESULT: allowed=true (whitelist rule)");
            break;
        case SyncFolderRuleType::WhiteListSubFolder:
            allowed = path != bestMatchExpandedPath;
            if (!allowed) {
                LOGW_INFO(Log::instance()->getLogger(), L"Path rejected by WhiteListSubFolder rule \""
                                                                << Utility::formatSyncPath(bestMatchExpandedPath) << L"\": "
                                                                << Utility::formatSyncPath(path));
            } else {
                LOG_DEBUG(Log::instance()->getLogger(),
                          "isSyncFolderAllowedByRules RESULT: allowed=true (whitelistsubfolder rule)");
            }
            break;
        case SyncFolderRuleType::None:
            break;
    }

    return ExitCode::Ok;
}

SyncPath SyncFolderAllowedChecker::expandRulePath(const SyncPath &rulePath) {
    QString pathStr = Path2QStr(rulePath);

    const QString homeDir = QDir::homePath();
    (void) pathStr.replace("$HOME", homeDir);

    QString user = qEnvironmentVariable("USER");
    if (user.isEmpty()) {
        user = qEnvironmentVariable("USERNAME");
    }
    if (!user.isEmpty()) {
        (void) pathStr.replace("$USER", user);
    }

    if (pathStr.startsWith("~")) {
        (void) pathStr.replace(0, 1, homeDir);
    }

    return QStr2Path(pathStr);
}

} // namespace KDC
