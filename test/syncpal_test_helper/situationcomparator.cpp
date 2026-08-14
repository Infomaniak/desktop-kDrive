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

#include "situationcomparator.h"

#include "syncpal/syncpal.h"

#include "libcommon/utility/utility.h"
#include "libcommonserver/log/log.h"
#include "jobs/network/kDrive_API/listing/csvfullfilelistwithcursorjob.h"
#include "test_utility/testhelpers.h"
#include "io/iohelper.h"

#include <Poco/JSON/Array.h>
#include <Poco/JSON/Object.h>

#include <sstream>
#include <unordered_map>

namespace KDC {

namespace {

//
// ─────────────────────────────────────────────────
// JSON Situation -> SituationMap
// ─────────────────────────────────────────────────
//
// Mirrors the walk InitialSituationSetter::addItem does for both supported formats (see initialsituationsetter.h),
// except it only ever accumulates a path/type/size in `out` instead of touching the real filesystem or drive.

void flatten(const Poco::JSON::Object::Ptr &obj, const SyncPath &parentPath, SituationMap &out) {
    std::vector<std::string> keys;
    obj->getNames(keys);

    for (const auto &key: keys) {
        const bool isDir = obj->isObject(key);
        const SyncPath path = parentPath / Str2SyncName(CommonUtility::toUpper(key));
        out.add(path, {isDir ? NodeType::Directory : NodeType::File,
                       isDir ? testhelpers::defaultDirSize : testhelpers::defaultFileSize});

        if (isDir) flatten(obj->getObject(key), path, out);
    }
}

void flatten(const Poco::JSON::Array::Ptr &arr, const SyncPath &parentPath, SituationMap &out) {
    for (size_t i = 0; i < arr->size(); ++i) {
        const auto &itemObj = arr->getObject(static_cast<uint64_t>(i));
        if (!itemObj) throw SituationGeneratorException("Extended format: each 'content' element must be an object");

        const std::string typeStr = itemObj->optValue<std::string>("type", "File");
        const NodeType type = (typeStr == "Directory") ? NodeType::Directory : NodeType::File;
        const std::string nameStr = itemObj->optValue<std::string>("name", "");
        if (nameStr.empty()) throw SituationGeneratorException("Extended format: missing 'name' field");

        const SyncPath path = parentPath / Str2SyncName(nameStr);
        const int64_t size = itemObj->optValue<int64_t>(
                "size", type == NodeType::File ? testhelpers::defaultFileSize : testhelpers::defaultDirSize);
        out.add(path, {type, size});

        if (type == NodeType::Directory && itemObj->isArray("content")) {
            flatten(itemObj->getArray("content"), path, out);
        }
    }
}

//
// ─────────────────────────────────────────────────
// CSV listing -> SituationMap
// ─────────────────────────────────────────────────
//
// A CSV listing (whether read from a file or fetched live) gives us, per item, only its own id/parentId/name/
// type/size - not its full path. Since a child can appear before its parent in the listing, we can't resolve
// paths on the fly while streaming: every item is buffered first, then paths are resolved from the complete
// id -> item map.

struct RawItem {
        NodeId parentId;
        std::string name;
        NodeType type = NodeType::Unknown;
        int64_t size = 0;
};

using RawItemMap = std::unordered_map<NodeId, RawItem, StringHashFunction, std::equal_to<>>;

using PathCache = std::unordered_map<NodeId, SyncPath, StringHashFunction, std::equal_to<>>;

SyncPath resolvePath(const NodeId &id, const RawItemMap &rawItems, PathCache &pathCache) {
    if (const auto it = pathCache.find(id); it != pathCache.end()) return it->second;

    const RawItem &item = rawItems.at(id);
    const bool isRoot = item.parentId.empty() || !rawItems.contains(item.parentId);
    const SyncPath path = isRoot ? SyncPath(Str2SyncName(item.name))
                                 : resolvePath(item.parentId, rawItems, pathCache) / Str2SyncName(item.name);
    return pathCache[id] = path;
}

// The root item itself (the one with no parent in `rawItems`) has no counterpart in a JSON Situation description
// (which only lists root's children), so it is used to resolve its children's paths but never added to the result.
SituationMap rawItemsToSituationMap(const RawItemMap &rawItems) {
    SituationMap SituationMap;
    PathCache pathCache;
    for (const auto &[id, item]: rawItems) {
        if (item.parentId.empty()) continue; // root, skip
        if (item.name.starts_with("tmpFile_")) continue; // trash, skip
        SituationMap.add(resolvePath(id, rawItems, pathCache), {item.type, item.size});
    }
    return SituationMap;
}

} // namespace

//
// ─────────────────────────────────────────────────
// SituationMap
// ─────────────────────────────────────────────────
//

void SituationMap::log() const {
    for (const auto &[path, info]: _items) {
        LOG_INFO(Log::instance()->getLogger(), path.string()
                                                       << " -> " << (info.type == NodeType::Directory ? "Directory" : "File")
                                                       << " (" << info.size << ")");
    }
}

//
// ─────────────────────────────────────────────────
// SituationComparator
// ─────────────────────────────────────────────────
//

SituationComparator::SituationComparator(const std::shared_ptr<SyncPal> syncPal) {
    setSyncpal(syncPal);
}

void SituationComparator::setSyncpal(const std::shared_ptr<SyncPal> syncPal) {
    _syncPal = syncPal;
}

SituationMap SituationComparator::jsonToSituationMap(const Situation &situation) {
    SituationMap SituationMap;
    if (const auto &obj = situation.jsonObject(); obj->isArray("content")) {
        flatten(obj->getArray("content"), {}, SituationMap);
    } else {
        // No "content" array: legacy format, where the object's own keys are the items.
        flatten(obj, {}, SituationMap);
    }
    return SituationMap;
}

SituationMap SituationComparator::csvToSituationMap(const std::string &csv) {
    SnapshotItemHandler handler(Log::instance()->getLogger());
    std::stringstream ss(csv);

    RawItemMap rawItems;
    SnapshotItem item;
    bool error = false;
    bool ignore = false;
    bool eof = false;
    while (handler.getItem(item, ss, error, ignore, eof) && !error) {
        if (!ignore) rawItems[item.id()] = {item.parentId(), SyncName2Str(item.name()), item.type(), item.size()};

        if (eof) break;
    }
    if (error) LOG_WARN(Log::instance()->getLogger(), "Error while parsing CSV listing.");

    return rawItemsToSituationMap(rawItems);
}

SituationMap SituationComparator::getRemoteSituation(const NodeId &remoteDirId /*= {}*/) const {
    if (!_syncPal) throw SituationGeneratorException("SituationComparator::getRemoteSituation: no SyncPal set");

    // An empty remoteDirId isn't "the sync root" for CsvFullFileListWithCursorJob: it lists the whole drive from
    // its actual root. Default to the SyncPal's own remote root node id so only the sync folder's content is
    // listed and compared against the (relative-to-sync-root) expected situation.
    const NodeId dirId = !remoteDirId.empty() ? remoteDirId : _syncPal->syncDb()->rootNode().nodeIdRemote().value_or(NodeId());

    CsvFullFileListWithCursorJob job(_syncPal->driveDbId(), dirId);
    if (const auto exitInfo = job.runSynchronously(); !exitInfo) {
        std::ostringstream oss;
        oss << "Error in CsvFullFileListWithCursorJob::runSynchronously: " << exitInfo;
        LOG_WARN(Log::instance()->getLogger(), oss.str());
        throw SituationGeneratorException(oss.str());
    }

    RawItemMap rawItems;
    SnapshotItem item;
    bool error = false;
    bool ignore = false;
    bool eof = false;
    while (job.getItem(item, error, ignore, eof)) {
        if (error) {
            const std::string msg = "Error while parsing CsvFullFileListWithCursorJob response.";
            LOG_WARN(Log::instance()->getLogger(), msg);
            throw SituationGeneratorException(msg);
        }
        if (!ignore) rawItems[item.id()] = {item.parentId(), SyncName2Str(item.name()), item.type(), item.size()};

        if (eof) break;
    }

    return rawItemsToSituationMap(rawItems);
}

SituationMap SituationComparator::getLocalSituation() const {
    SituationMap SituationMap;

    if (!_syncPal) return SituationMap;

    const SyncPath rootPath = _syncPal->localPath();

    IoHelper::DirectoryIterator dirIt;
    IoError ioError = IoError::Success;

    if (!IoHelper::getRecursiveDirectoryIterator(rootPath, ioError, dirIt)) {
        LOG_WARN(Log::instance()->getLogger(), "Failed to create DirectoryIterator for local path: " << rootPath.string());
        return SituationMap;
    }

    DirectoryEntry entry;
    bool endOfDirectory = false;
    bool exceptionOccurred = false;

    while (dirIt.next(entry, endOfDirectory, ioError) && !endOfDirectory) {
        std::error_code ec;
        const SyncPath relativePath = std::filesystem::relative(entry.path(), rootPath, ec);
        if (ec || relativePath.empty()) {
            exceptionOccurred = true;
            continue;
        }

        // Skip trash-like temp files, same as the remote side.
        if (relativePath.filename().string().starts_with("tmpFile_")) continue;
        if (relativePath.filename().string().starts_with(".kDrive-cache")) continue;

        SituationMap::ItemInfo info;
        if (entry.is_directory(ec)) {
            info.type = NodeType::Directory;
            info.size = testhelpers::defaultDirSize;
        } else if (entry.is_regular_file(ec)) {
            info.type = NodeType::File;
            info.size = static_cast<int64_t>(entry.file_size(ec));
        } else {
            continue; // Skip symlinks / special files.
        }

        if (ec) {
            exceptionOccurred = true;
            continue;
        }

        SituationMap.add(relativePath, info);
    }

    const auto exitInfo = IoHelper::checkDirectoryIteratorInterruption(endOfDirectory, ioError, entry, exceptionOccurred);
    if (!exitInfo) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Directory iteration did not complete cleanly for local path: " << rootPath.string());
    }

    return SituationMap;
}

bool SituationComparator::compareLocal(const Situation &expectedLocalSituation) const {
    const SituationMap expected = jsonToSituationMap(expectedLocalSituation);
    const SituationMap actual = getLocalSituation();
    return expected == actual;
}

bool SituationComparator::compareRemote(const Situation &expectedRemoteSituation) const {
    const SituationMap expected = jsonToSituationMap(expectedRemoteSituation);
    const SituationMap actual = getRemoteSituation();
    return expected == actual;
}

bool SituationComparator::compareSituation(const Situation &expectedLocalSituation, const Situation &expectedRemoteSituation) const {
    return compareLocal(expectedLocalSituation) && compareRemote(expectedRemoteSituation);
}

} // namespace KDC
