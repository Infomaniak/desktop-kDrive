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

#include "utility/types.h"

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>

namespace KDC {

class SyncPal;

class SituationGeneratorException final : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
};

/**
 * @brief Very simple map wrapper: remote/local node id -> item name. Used to represent a flat listing of items
 * (either the expected situation parsed from a JSON description, or the actual situation read from a CSV listing).
 */
class SituationCSV {
    public:
        using ItemMap = std::map<int64_t, std::string>; // node id -> item name

        SituationCSV() = default;

        void add(int64_t id, const std::string &name) { _items[id] = name; }

        [[nodiscard]] const ItemMap &items() const noexcept { return _items; }
        [[nodiscard]] size_t size() const noexcept { return _items.size(); }

        bool operator==(const SituationCSV &other) const noexcept = default;

        void log() const;

    private:
        ItemMap _items;
};

/**
 * @brief Wraps a JSON description of a local or remote directory situation.
 */
class Situation {
    public:
        using StringType = std::filesystem::path::string_type;

        explicit Situation(const StringType &jsonDescription);

        // Reads the JSON from a file instead of an inline string. Throws the same way the
        // constructor does if the content isn't valid.
        [[nodiscard]] static Situation fromFile(const std::filesystem::path &filePath);

        const StringType &json() const noexcept;

        bool operator==(const Situation &other) const noexcept = default;

        void log() const;

    private:
        StringType _jsonDescription;
};

/**
 * @brief Compares an expected situation (described as JSON) against the real local and remote situations.
 *
 * Usage: construct with the expected local and remote `Situation` descriptions, then call `compareRemote` /
 * `compareLocal` (or `compare` for both) against a running `SyncPal` to fetch the real situations and diff them
 * with the expected ones.
 */
class GetSituation {
    public:
        GetSituation() = default;
        GetSituation(const Situation &expectedLocalSituation, const Situation &expectedRemoteSituation);

        void setSyncpal(std::shared_ptr<SyncPal> syncPal);

        // Converts a JSON `Situation` description into the flat `SituationCSV` representation used for comparison.
        [[nodiscard]] static SituationCSV jsonToSituationCSV(const Situation &situation);

        // Parses a raw CSV listing (same format as returned by CsvFullFileListWithCursorJob) into a `SituationCSV`.
        [[nodiscard]] static SituationCSV csvToSituationCSV(const std::string &csv);

        // Fetches the real remote situation by running a CsvFullFileListWithCursorJob against `remoteDirId`
        // (or the drive root if empty).
        [[nodiscard]] SituationCSV getRemoteSituation(const NodeId &remoteDirId = {}) const;

        // PLACEHOLDER: fetches the real local situation by scanning the local sync folder. Not yet implemented,
        // the exact strategy (which ids to use, how to walk the tree) still needs to be decided.
        [[nodiscard]] SituationCSV getLocalSituation() const;

        // Compares the expected remote situation against the real one fetched from the server.
        [[nodiscard]] bool compareRemote() const;

        // PLACEHOLDER: compares the expected local situation against the real one on disk.
        [[nodiscard]] bool compareLocal() const;

        // Runs both comparisons.
        [[nodiscard]] bool compare() const;

    private:
        std::shared_ptr<SyncPal> _syncPal;
        Situation _expectedLocalSituation{Str2SyncName("{}")};
        Situation _expectedRemoteSituation{Str2SyncName("{}")};
};

} // namespace KDC
