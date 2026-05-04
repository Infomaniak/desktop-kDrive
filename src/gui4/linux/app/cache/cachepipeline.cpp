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

#include "libcommon/utility/cstypes.h"

#include <QLoggingCategory>

#include <tuple>

namespace KDC {

namespace {
Q_LOGGING_CATEGORY(lcCachePipeline, "gui.v4.cachepipeline", QtInfoMsg)

template<typename Signal, typename Slot>
struct CacheConnection {
        const char *signalName;
        Signal signal;
        Slot slot;
};

template<typename Signal, typename Slot>
constexpr auto makeCacheConnection(const char *const signalName, Signal signal, Slot slot) {
    return CacheConnection<Signal, Slot>{signalName, signal, slot};
}

constexpr auto directCacheConnections =
        std::make_tuple(makeCacheConnection("userAdded", &CommService::userAdded, &AppCache::upsertUser),
                        makeCacheConnection("userUpdated", &CommService::userUpdated, &AppCache::upsertUser),
                        makeCacheConnection("userRemoved", &CommService::userRemoved, &AppCache::removeUser),
                        makeCacheConnection("accountAdded", &CommService::accountAdded, &AppCache::upsertAccount),
                        makeCacheConnection("accountUpdated", &CommService::accountUpdated, &AppCache::upsertAccount),
                        makeCacheConnection("accountRemoved", &CommService::accountRemoved, &AppCache::removeAccount),
                        makeCacheConnection("driveAdded", &CommService::driveAdded, &AppCache::upsertDrive),
                        makeCacheConnection("driveUpdated", &CommService::driveUpdated, &AppCache::upsertDrive),
                        makeCacheConnection("driveRemoved", &CommService::driveRemoved, &AppCache::removeDrive),
                        makeCacheConnection("syncAdded", &CommService::syncAdded, &AppCache::upsertSync),
                        makeCacheConnection("syncUpdated", &CommService::syncUpdated, &AppCache::upsertSync),
                        makeCacheConnection("syncRemoved", &CommService::syncRemoved, &AppCache::removeSync));
} // namespace

CachePipeline::CachePipeline(CommService &commService, AppCache &appCache, QObject *const parent) :
    QObject(parent),
    _commService(commService),
    _appCache(appCache) {
    connectDropPipeline();
}

void CachePipeline::connectDropPipeline() {
    std::apply(
            [this](const auto &...connection) {
                (_preHydrationConnections.push_back(
                         connect(&_commService, connection.signal, this,
                                 [signalName = connection.signalName](const auto &...) { logDroppedPush(signalName); })),
                 ...);
            },
            directCacheConnections);

    _preHydrationConnections.push_back(
            connect(&_commService, &CommService::errorAdded, this, [](const ErrorInfo &) { logDroppedPush("errorAdded"); }));
    _preHydrationConnections.push_back(
            connect(&_commService, &CommService::errorRemoved, this, [](const ErrorDbId) { logDroppedPush("errorRemoved"); }));
}

void CachePipeline::connectLivePipeline() {
    std::apply(
            [this](const auto &...connection) {
                ((void) connect(&_commService, connection.signal, &_appCache, connection.slot, Qt::UniqueConnection), ...);
            },
            directCacheConnections);

    (void) connect(&_commService, &CommService::errorAdded, this, [this](const ErrorInfo &info) {
        if (info.level() == ErrorLevel::Server) {
            _appCache.upsertServerError(info);
            return;
        }
        _appCache.upsertSyncError(info);
    });
    (void) connect(&_commService, &CommService::errorRemoved, this, [this](const ErrorDbId errorDbId) {
        if (_appCache.syncError(errorDbId).has_value()) {
            _appCache.removeSyncError(errorDbId);
            return;
        }
        if (_appCache.serverError(errorDbId).has_value()) {
            _appCache.removeServerError(errorDbId);
        }
    });
}

void CachePipeline::markHydrated() {
    if (_hydrated) {
        return;
    }

    _hydrated = true;
    for (const auto &connection: _preHydrationConnections) {
        (void) QObject::disconnect(connection);
    }
    _preHydrationConnections.clear();
    connectLivePipeline();
    qCInfo(lcCachePipeline) << "Cache hydration completed; live cache push mutations enabled";
}

void CachePipeline::logDroppedPush(const char *const signalName) {
    qCWarning(lcCachePipeline) << "Cache push dropped before hydration completed | signal:" << signalName;
}

} // namespace KDC
