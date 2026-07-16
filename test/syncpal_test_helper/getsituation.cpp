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

#include "getsituation.h"

#include "syncpal/syncpal.h"

#include "libcommonserver/log/log.h"
#include "jobs/network/kDrive_API/listing/csvfullfilelistwithcursorjob.h"

#include <Poco/JSON/Parser.h>

#include <fstream>
#include <sstream>

namespace KDC {

//
// ─────────────────────────────────────────────────
// SituationCSV
// ─────────────────────────────────────────────────
//

void SituationCSV::log() const {
    for (const auto &[id, name]: _items) {
        LOG_INFO(Log::instance()->getLogger(), id << " -> " << name);
    }
}

//
// ─────────────────────────────────────────────────
// Situation
// ─────────────────────────────────────────────────
//

Situation::Situation(const StringType &jsonDescription) :
    _jsonDescription(jsonDescription) {
    try {
        Poco::JSON::Parser parser;
        (void) parser.parse(SyncName2Str(jsonDescription)).extract<Poco::JSON::Object::Ptr>();
    } catch (Poco::Exception &) {
        throw SituationGeneratorException("Invalid Situation JSON");
    }
}

Situation Situation::fromFile(const std::filesystem::path &filePath) {
    const std::ifstream file(filePath, std::ios::binary);
    if (!file) throw SituationGeneratorException("Situation::fromFile: unable to open file: " + filePath.string());

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return Situation(Str2SyncName(buffer.str()));
}

const Situation::StringType &Situation::json() const noexcept {
    return _jsonDescription;
}

void Situation::log() const {
    LOGW_INFO(Log::instance()->getLogger(), SyncName2WStr(_jsonDescription));
}

//
// ─────────────────────────────────────────────────
// GetSituation
// ─────────────────────────────────────────────────
//

GetSituation::GetSituation(const Situation &expectedLocalSituation, const Situation &expectedRemoteSituation) :
    _expectedLocalSituation(expectedLocalSituation),
    _expectedRemoteSituation(expectedRemoteSituation) {}

void GetSituation::setSyncpal(const std::shared_ptr<SyncPal> syncPal) {
    _syncPal = syncPal;
}

SituationCSV GetSituation::jsonToSituationCSV(const Situation & /*situation*/) {
    // PLACEHOLDER: the JSON description (see SetInitialSituation-like formats) only carries lowercase ids/names,
    // not the real int64_t node ids assigned by the server. We still need to decide how to turn this into a
    // SituationCSV that can be meaningfully compared to the real (id -> name) map fetched from the server/disk
    // (e.g. compare by name only, or resolve real ids via the id mapping built while generating the situation).
    return {};
}

SituationCSV GetSituation::csvToSituationCSV(const std::string &csv) {
    SituationCSV situationCSV;

    SnapshotItemHandler handler(Log::instance()->getLogger());
    std::stringstream ss(csv);

    SnapshotItem item;
    bool error = false;
    bool ignore = false;
    bool eof = false;
    while (handler.getItem(item, ss, error, ignore, eof)) {
        if (error) {
            LOG_WARN(Log::instance()->getLogger(), "Error while parsing CSV listing.");
            break;
        }
        if (ignore) continue;

        try {
            situationCSV.add(std::stoll(item.id()), SyncName2Str(item.name()));
        } catch (const std::exception &) {
            LOG_WARN(Log::instance()->getLogger(), "Unable to convert item id '" << item.id() << "' to int64_t.");
        }

        if (eof) break;
    }

    return situationCSV;
}

SituationCSV GetSituation::getRemoteSituation(const NodeId &remoteDirId /*= {}*/) const {
    if (!_syncPal) return {};

    CsvFullFileListWithCursorJob job(_syncPal->driveDbId(), remoteDirId);
    if (const auto exitInfo = job.runSynchronously(); !exitInfo) {
        LOG_WARN(Log::instance()->getLogger(), "Error in CsvFullFileListWithCursorJob::runSynchronously: " << exitInfo);
        return {};
    }

    SituationCSV situationCSV;
    SnapshotItem item;
    bool error = false;
    bool ignore = false;
    bool eof = false;
    while (job.getItem(item, error, ignore, eof)) {
        if (error) {
            LOG_WARN(Log::instance()->getLogger(), "Error while parsing CsvFullFileListWithCursorJob response.");
            break;
        }
        if (ignore) continue;

        try {
            situationCSV.add(std::stoll(item.id()), SyncName2Str(item.name()));
        } catch (const std::exception &) {
            LOG_WARN(Log::instance()->getLogger(), "Unable to convert item id '" << item.id() << "' to int64_t.");
        }

        if (eof) break;
    }

    return situationCSV;
}

SituationCSV GetSituation::getLocalSituation() const {
    // PLACEHOLDER: needs to walk the local sync folder (_syncPal->localPath()) recursively and build a
    // SituationCSV out of it. The id to use (local node id from the DB? filesystem id?) still needs to be decided.
    return {};
}

bool GetSituation::compareRemote() const {
    const SituationCSV expected = jsonToSituationCSV(_expectedRemoteSituation);
    const SituationCSV actual = getRemoteSituation();
    return expected == actual;
}

bool GetSituation::compareLocal() const {
    const SituationCSV expected = jsonToSituationCSV(_expectedLocalSituation);
    const SituationCSV actual = getLocalSituation();
    return expected == actual;
}

bool GetSituation::compare() const {
    return compareLocal() && compareRemote();
}

} // namespace KDC
