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

#include "initialsituationsetter.h" // Situation, SituationGeneratorException
#include "utility/types.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>

namespace KDC {

class SyncPal;

/**
 * @brief Very simple map wrapper: item relative path (from the sync root) -> {type, size}. Used to represent a
 * flat listing of items (either the expected situation parsed from a JSON `Situation` description, or the actual
 * situation read from a CSV listing / walked from disk).
 *
 * Keyed by path rather than node id on purpose: the id in a JSON `Situation` description is just a local token
 * `InitialSituationSetter` uses internally to wire up parent/child relationships while generating the situation
 * (see initialsituationsetter.h) - it has no relationship to the real int64_t node id the server (or the local
 * DB) later assigns. Path is the one thing both a JSON description and a real listing can express in common.
 */
class SituationMap {
    public:
        struct ItemInfo {
                NodeType type = NodeType::Unknown;
                int64_t size = 0; // Meaningful for files. Directories get testhelpers::defaultDirSize.
                bool operator==(const ItemInfo &other) const noexcept = default;
        };
        using ItemMap = std::map<SyncPath, ItemInfo>; // item relative path -> {type, size}

        SituationMap() = default;

        void add(const SyncPath &path, const ItemInfo &info) { _items[path] = info; }

        [[nodiscard]] const ItemMap &items() const noexcept { return _items; }
        [[nodiscard]] size_t size() const noexcept { return _items.size(); }

        bool operator==(const SituationMap &other) const noexcept = default;

        void log() const;

    private:
        ItemMap _items;
};

/**
 * @brief Compares an expected situation (described as JSON, see Situation in initialsituationsetter.h) against
 * the real local and remote situations.
 *
 * Usage: construct (optionally with a SyncPal, or set one via setSyncpal), then call `compareSituation` with the
 * expected local and remote `Situation` descriptions to diff them against the real ones.
 */
class SituationComparator {
    public:
        SituationComparator() = default;
        explicit SituationComparator(std::shared_ptr<SyncPal> syncPal);

        void setSyncpal(std::shared_ptr<SyncPal> syncPal);

        // Converts a JSON `Situation` description into the flat `SituationMap` representation (relative path ->
        // {type, size}) used for comparison. A size left unspecified in the JSON is defaulted the same way
        // InitialSituationSetter does (testhelpers::defaultFileSize / defaultDirSize), so that comparing against
        // a situation generated from the same description lines up.
        [[nodiscard]] static SituationMap jsonToSituationMap(const Situation &situation);

        // Parses a raw CSV listing (same format as returned by CsvFullFileListWithCursorJob) into a `SituationMap`.
        [[nodiscard]] static SituationMap csvToSituationMap(const std::string &csv);

        // Fetches the real remote situation by running a CsvFullFileListWithCursorJob against `remoteDirId`
        // (or the drive root if empty).
        [[nodiscard]] SituationMap getRemoteSituation(const NodeId &remoteDirId = {}) const;

        // fetches the real local situation by scanning the local sync folder.
        [[nodiscard]] SituationMap getLocalSituation() const;

        // Compares expectedLocalSituation / expectedRemoteSituation (same JSON format as InitialSituationSetter)
        // against the real local / remote situations.
        [[nodiscard]] bool compareSituation(const Situation &expectedLocalSituation,
                                            const Situation &expectedRemoteSituation) const;

    private:
        [[nodiscard]] bool compareLocal(const Situation &expectedLocalSituation) const;
        [[nodiscard]] bool compareRemote(const Situation &expectedRemoteSituation) const;

        std::shared_ptr<SyncPal> _syncPal;
};

} // namespace KDC
