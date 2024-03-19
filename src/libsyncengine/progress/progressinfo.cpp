/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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

ProgressInfo::ProgressInfo(std::shared_ptr<SyncPal> syncPal)
    : _syncPal(syncPal), _totalSizeOfCompletedJobs(0), _maxFilesPerSecond(0), _maxBytesPerSecond(0), _update(false) {
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
    _maxBytesPerSecond = 2000000.0;  // 2 MB/s
    _maxFilesPerSecond = 10.0;

    _update = false;
}

static bool shouldCountProgress(const SyncFileItem &item) {
    const auto instruction = item.instruction();

    // Skip any ignored, error or non-propagated files and directories.
    if (instruction == SyncFileInstructionNone || instruction == SyncFileInstructionUpdateMetadata ||
        instruction == SyncFileInstructionIgnore) {
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
    for (auto &item : _currentItems) {
        if (item.second.empty()) {
            continue;
        }
        item.second.front().progress().update();
    }

    _maxFilesPerSecond = std::max(_fileProgress.progressPerSec(), _maxFilesPerSecond);
    _maxBytesPerSecond = std::max(_sizeProgress.progressPerSec(), _maxBytesPerSecond);
}

void ProgressInfo::initProgress(const SyncFileItem &item) {
    SyncPath path = item.newPath().has_value() ? item.newPath().value() : item.path();
    ProgressItem progressItem;
    progressItem.setItem(item);
    progressItem.progress().setTotal(item.size());
    progressItem.progress().setCompleted(0);

    _currentItems[path].push(progressItem);

    _fileProgress.setTotal(_fileProgress.total() + 1);
    _sizeProgress.setTotal(_sizeProgress.total() + item.size());
}

bool ProgressInfo::getSyncFileItem(const SyncPath &path, SyncFileItem &item) {
    auto it = _currentItems.find(path);
    if (_currentItems.find(path) == _currentItems.end() || it->second.empty()) {
        return false;
    }
    item = it->second.front().item();
    return true;
}

void ProgressInfo::setProgress(const SyncPath &path, int64_t completed) {
    auto it = _currentItems.find(path);
    if (it == _currentItems.end() || it->second.empty()) {
        return;
    }

    SyncFileItem &item = it->second.front().item();
    if (!shouldCountProgress(item)) {
        return;
    }

    it->second.front().progress().setCompleted(completed);
    recomputeCompletedSize();
}

void ProgressInfo::setProgressComplete(const SyncPath &path, SyncFileStatus status) {
    auto it = _currentItems.find(path);
    if (it == _currentItems.end() || it->second.empty()) {
        return;
    }

    SyncFileItem &item = it->second.front().item();
    item.setStatus(status);

    _syncPal->addCompletedItem(_syncPal->syncDbId(), item);

    if (!shouldCountProgress(item)) {
        return;
    }

    _fileProgress.setCompleted(_fileProgress.completed() + 1);
    if (ProgressInfo::isSizeDependent(item)) {
        _totalSizeOfCompletedJobs += it->second.front().progress().total();
    }

    it->second.pop();
    if (it->second.empty()) {
        _currentItems.erase(path);
    }
    recomputeCompletedSize();
}

bool ProgressInfo::isSizeDependent(const SyncFileItem &item) const {
    return !item.isDirectory() &&
           (item.instruction() == SyncFileInstructionUpdate || item.instruction() == SyncFileInstructionGet ||
            item.instruction() == SyncFileInstructionPut) &&
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
    size.setEstimatedEta(int64_t((1.0 - beOptimistic) * size.estimatedEta() + beOptimistic * optimisticEta()));

    return size;
}

int64_t ProgressInfo::optimisticEta() const {
    return static_cast<int64_t>(_fileProgress.remaining() / _maxFilesPerSecond * 1000 +
                                _sizeProgress.remaining() / _maxBytesPerSecond * 1000);
}

bool ProgressInfo::trustEta() const {
    return totalProgress().estimatedEta() < 100 * optimisticEta();
}

Estimates ProgressInfo::fileProgress(const SyncFileItem &item) {
    auto it = _currentItems.find(item.path());
    if (it == _currentItems.end() || it->second.empty()) {
        return Estimates();
    }

    return it->second.front().progress().estimates();
}

void ProgressInfo::recomputeCompletedSize() {
    int64_t r = _totalSizeOfCompletedJobs;
    for (auto &itemElt : _currentItems) {
        if (isSizeDependent(itemElt.second.front().item())) {
            r += itemElt.second.front().progress().completed();
        }
    }
    _sizeProgress.setCompleted(r);
}

}  // namespace KDC
