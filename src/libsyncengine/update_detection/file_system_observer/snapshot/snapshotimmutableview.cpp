#include "snapshotimmutableview.h"
namespace KDC {
SnapshotImmutableView::SnapshotImmutableView(SnapshotView const &) {}
bool SnapshotImmutableView::path(const NodeId &itemId, SyncPath &path, bool &ignore) const noexcept {
    path.clear();
    ignore = false;

    if (itemId.empty()) {
        LOG_WARN(Log::instance()->getLogger(), "Error in Snapshot::path: empty item ID argument.");
        return false;
    }

    bool ok = true;
    std::deque<std::pair<NodeId, SyncName>> ancestors;
    {
        bool parentIsRoot = false;
        NodeId id = itemId;
        const std::scoped_lock lock(_mutex);
        while (!parentIsRoot) {
            if (const auto item = findItem(id); item) {
                if (!item->path().empty()) {
                    path = item->path();
                    break;
                }
                ancestors.push_back({item->id(), item->name()});
                id = item->parentId();
                parentIsRoot = id == rootFolderId();
                continue;
            }

            ok = false;
            break;
        }
    }

    // Construct path
    SyncPath tmpParentPath(path);
    while (!ancestors.empty()) {
        path /= ancestors.back().second;
        const auto item = findItem(ancestors.back().first);
        assert(item);
        item->setPath(path);
        ancestors.pop_back();
    }

    // Trick to ignore items with a pattern like "X:" in their names on Windows.
    // Since only relative path are stored in the liveSnapshot, the root name should always be empty.
    // If it is not empty, that means that the item name is invalid.
    if (!path.root_name().empty()) {
        ignore = true;
        return false;
    }

    return ok;
}

} // namespace KDC
