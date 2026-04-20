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

#pragma once

#include "app/cache/appcache.h"
#include "app/services/commservice.h"

#include <QObject>

namespace KDC {

/**
 * Owns all signal connections from CommService to AppCache.
 *
 * This is the single component responsible for keeping AppCache fed with
 * server-push events. No service or other object should connect CommService
 * signals to AppCache directly.
 */
class CachePipeline : public QObject {
        Q_OBJECT

    public:
        explicit CachePipeline(CommService &commService, AppCache &appCache, QObject *parent = nullptr);

    private:
        CommService &_commService;
        AppCache &_appCache;
};

} // namespace KDC
