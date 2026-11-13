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

#include <QString>

#include <optional>

/**
 * Matomo taxonomy of the user action log ids.
 *
 * Categories and event names are the ones of the kDrive v4 desktop application (`homePage`, `openSettings`, ...), so
 * a Matomo report groups them across platforms. An id absent from the catalog is only logged.
 */
namespace KDC::AnalyticsCatalog {

struct Event {
        QString category;
        QString name;
        // Detail whose boolean (1 or 0) or number becomes the event value; empty when the event has no value.
        QString valueDetail;
};

[[nodiscard]] std::optional<Event> eventForAction(const QString &actionId);
/** Page view category of a screen or dialog id. */
[[nodiscard]] std::optional<QString> pageViewForScreen(const QString &screenId);

} // namespace KDC::AnalyticsCatalog
