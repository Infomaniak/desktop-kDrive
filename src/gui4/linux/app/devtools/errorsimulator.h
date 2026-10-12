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

class QObject;

namespace KDC {

class CachePopulator;
class MainSelectionStore;
class SignalDispatcher;

// Debug builds only. When KDRIVE_SIMULATE_ERRORS is set, injects fake errors through the real UTILITY_ERROR_ADDED path
// once the cache is populated: `all` (every non-blocking kind and 3 conflicts), a blocking error (`asleep`, `wakingup`,
// `maintenance`, `notrenew`, `accessdenied`, `loggedout`) or `update-required`.
void installErrorSimulator(const CachePopulator &cachePopulator, const SignalDispatcher &signalDispatcher,
                           const MainSelectionStore &selectionStore, const QObject *context);

} // namespace KDC
