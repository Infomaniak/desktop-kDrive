/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
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

#include "progressinfo.h"
#include "syncpal/syncpal.h"

namespace KDC {

ProgressInfo::ProgressInfo(std::shared_ptr<SyncPal> syncPal) :
    _syncPal(syncPal) {
    reset();
}

ProgressInfo::~ProgressInfo() {
    LOG_DEBUG(Log::instance()->getLogger(), "~ProgressInfo");
}

void ProgressInfo::reset() {
    _currentItems.clear();
    _sizeProgress = Progress();
    _fileProgress = Progress();
    _totalSizeOfCompletedJobs = 0;

    // Historically, these starting estimates were way lower, but that lead
    // to gross overestimation of ETA when a good estimate wasn't available.
    _maxBytesPerSecond = 2000000.0; // 2 MB/s
    _maxFilesPerSecond = 10.0;

    _update = false;
}

static bool shouldCountProgress(const SyncFileItem &item) {
    const auto instruction = item.instruction();

    // Skip any ignored, error or non-propagated files and directories.
    if (instruction == SyncFileInstruction::None || instruction == SyncFileInstruction::UpdateMetadata ||
        instruction == SyncFileInstruction::Ignore) {
        return false;
    }

    return true;
}

void ProgressInfo::updateEstimates() {
    if (!_update) {
        return;
    }

    _sizeProgress.update();
    _fileProgress.update();

    // Update progress of all running items.
    for (auto &item: _currentItems) {
        if (item.second.empty()) {
            continue;
        }
        item.second.front().progress().update();
    }

    _maxFilesPerSecond = std::max(_fileProgress.progressPerSec(), _maxFilesPerSecond);
    _maxBytesPerSecond = std::max(_sizeProgress.progressPerSec(), _maxBytesPerSecond);
}

bool ProgressInfo::initProgress(const SyncFileItem &item) {
    const SyncPath path = item.newPath().has_value() ? item.newPath().value() : item.path();
    ProgressItem progressItem;
    progressItem.setItem(item);
    progressItem.progress().setTotal(item.size());
    progressItem.progress().setCompleted(0);

    SyncPath normalizedPath;
    if (!Utility::normalizedSyncPath(path, normalizedPath)) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in Utility::normalizedSyncPath: " << Utility::formatSyncPath(path));
        return false;
    }

    _currentItems[normalizedPath].push(progressItem);

    _fileProgress.setTotal(_fileProgress.total() + 1);
    _sizeProgress.setTotal(_sizeProgress.total() + item.size());
    return true;
}

bool ProgressInfo::getSyncFileItem(const SyncPath &path, SyncFileItem &item) {
    SyncPath normalizedPath;
    if (!Utility::normalizedSyncPath(path, normalizedPath)) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in Utility::normalizedSyncPath: " << Utility::formatSyncPath(path));
        return false;
    }

    const auto it = _currentItems.find(normalizedPath);
    if (it == _currentItems.end() || it->second.empty()) {
        return false;
    }
    item = it->second.front().item();
    return true;
}

bool ProgressInfo::setProgress(const SyncPath &path, const int64_t completed) {
    SyncPath normalizedPath;
    if (!Utility::normalizedSyncPath(path, normalizedPath)) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in Utility::normalizedSyncPath: " << Utility::formatSyncPath(path));
        return false;
    }

    const auto it = _currentItems.find(normalizedPath);
    if (it == _currentItems.end() || it->second.empty()) {
        return true;
    }

    if (const SyncFileItem &item = it->second.front().item(); !shouldCountProgress(item)) {
        return true;
    }

    it->second.front().progress().setCompleted(completed);
    recomputeCompletedSize();
    return true;
}

bool ProgressInfo::setProgressComplete(const SyncPath &path, const SyncFileStatus status) {
    SyncPath normalizedPath;
    if (!Utility::normalizedSyncPath(path, normalizedPath)) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in Utility::normalizedSyncPath: " << Utility::formatSyncPath(path));
        return false;
    }

    const auto it = _currentItems.find(normalizedPath);
    if (it == _currentItems.end() || it->second.empty()) {
        LOGW_INFO(Log::instance()->getLogger(),
                  L"Item not found in ProgressInfo list (normal for ommited operation): " << Utility::formatSyncPath(path));
        return true;
    }

    SyncFileItem &item = it->second.front().item();
    item.setStatus(status);

    _syncPal->addCompletedItem(_syncPal->syncDbId(), item);

    if (!shouldCountProgress(item)) {
        return true;
    }

    _fileProgress.setCompleted(_fileProgress.completed() + 1);
    if (ProgressInfo::isSizeDependent(item)) {
        _totalSizeOfCompletedJobs += it->second.front().progress().total();
    }

    it->second.pop();
    if (it->second.empty()) {
        _currentItems.erase(normalizedPath);
    }

    if (const auto currentItemsSize = _currentItems.size(); currentItemsSize < 100 || currentItemsSize % 1000 == 0) {
        recomputeCompletedSize();
    }
    return true;
}

bool ProgressInfo::setSyncFileItemRemoteId(const SyncPath &path, const NodeId &remoteId) {
    SyncPath normalizedPath;
    if (!Utility::normalizedSyncPath(path, normalizedPath)) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in Utility::normalizedSyncPath: " << Utility::formatSyncPath(path));
        return false;
    }
    const auto it = _currentItems.find(normalizedPath);
    if (it == _currentItems.end() || it->second.empty()) {
        LOGW_INFO(Log::instance()->getLogger(),
                  L"Item not found in ProgressInfo list (normal for omitted operation): " << Utility::formatSyncPath(path));
        return true;
    }

    it->second.front().item().setRemoteNodeId(remoteId);
    return true;
}

bool ProgressInfo::isSizeDependent(const SyncFileItem &item) const {
    return !item.isDirectory() &&
           (item.instruction() == SyncFileInstruction::Update || item.instruction() == SyncFileInstruction::Get ||
            item.instruction() == SyncFileInstruction::Put) &&
           !item.dehydrated();
}

Estimates ProgressInfo::totalProgress() const {
    Estimates file = _fileProgress.estimates();
    if (_sizeProgress.total() == 0) {
        return file;
    }

    Estimates size = _sizeProgress.estimates();

    // Compute a value that is 0 when fps is <=L*max and 1 when fps is >=U*max
    double fps = _fileProgress.progressPerSec();
    double fpsL = 0.5;
    double fpsU = 0.8;
    double nearMaxFps = std::max(0.0, std::min((fps - fpsL * _maxFilesPerSecond) / ((fpsU - fpsL) * _maxFilesPerSecond), 1.0));

    // Compute a value that is 0 when transfer is >= U*max and 1 when transfer is <= L*max
    double trans = _sizeProgress.progressPerSec();
    double transU = 0.1;
    double transL = 0.01;
    double slowTransfer =
            1.0 - std::max(0.0, std::min((trans - transL * _maxBytesPerSecond) / ((transU - transL) * _maxBytesPerSecond), 1.0));

    double beOptimistic = nearMaxFps * slowTransfer;
    size.setEstimatedEta(static_cast<int64_t>(((1.0 - beOptimistic) * static_cast<double>(size.estimatedEta()) +
                                               beOptimistic * static_cast<double>(optimisticEta()))));

    return size;
}

int64_t ProgressInfo::optimisticEta() const {
    return static_cast<int64_t>(static_cast<double>(_fileProgress.remaining()) / _maxFilesPerSecond * 1000 +
                                static_cast<double>(_sizeProgress.remaining()) / _maxBytesPerSecond * 1000);
}

bool ProgressInfo::trustEta() const {
    return totalProgress().estimatedEta() < 100 * optimisticEta();
}

void ProgressInfo::recomputeCompletedSize() {
    int64_t r = _totalSizeOfCompletedJobs;
    for (auto &itemElt: _currentItems) {
        if (isSizeDependent(itemElt.second.front().item())) {
            r += itemElt.second.front().progress().completed();
        }
    }
    _sizeProgress.setCompleted(r);
}

} // namespace KDC
