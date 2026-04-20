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

#include "cachepipeline.h"

namespace KDC {

CachePipeline::CachePipeline(CommService &commService, AppCache &appCache, QObject *const parent) :
    QObject(parent),
    _commService(commService),
    _appCache(appCache) {
    // --- User ---
    (void) connect(&_commService, &CommService::userAdded, &_appCache, &AppCache::upsertUser, Qt::UniqueConnection);
    (void) connect(&_commService, &CommService::userUpdated, &_appCache, &AppCache::upsertUser, Qt::UniqueConnection);
    (void) connect(&_commService, &CommService::userRemoved, &_appCache, &AppCache::removeUser, Qt::UniqueConnection);

    // --- Account ---
    (void) connect(&_commService, &CommService::accountAdded, &_appCache, &AppCache::upsertAccount, Qt::UniqueConnection);
    (void) connect(&_commService, &CommService::accountUpdated, &_appCache, &AppCache::upsertAccount, Qt::UniqueConnection);
    (void) connect(&_commService, &CommService::accountRemoved, &_appCache, &AppCache::removeAccount, Qt::UniqueConnection);

    // --- Drive ---
    (void) connect(&_commService, &CommService::driveAdded, &_appCache, &AppCache::upsertDrive, Qt::UniqueConnection);
    (void) connect(&_commService, &CommService::driveUpdated, &_appCache, &AppCache::upsertDrive, Qt::UniqueConnection);
    (void) connect(&_commService, &CommService::driveRemoved, &_appCache, &AppCache::removeDrive, Qt::UniqueConnection);

    // --- Sync ---
    (void) connect(&_commService, &CommService::syncAdded, &_appCache, &AppCache::upsertSync, Qt::UniqueConnection);
    (void) connect(&_commService, &CommService::syncUpdated, &_appCache, &AppCache::upsertSync, Qt::UniqueConnection);
    (void) connect(&_commService, &CommService::syncRemoved, &_appCache, &AppCache::removeSync, Qt::UniqueConnection);

    // --- Error ---
    (void) connect(&_commService, &CommService::errorAdded, &_appCache, &AppCache::upsertError, Qt::UniqueConnection);
    (void) connect(&_commService, &CommService::errorRemoved, &_appCache, &AppCache::removeError, Qt::UniqueConnection);
}

} // namespace KDC
