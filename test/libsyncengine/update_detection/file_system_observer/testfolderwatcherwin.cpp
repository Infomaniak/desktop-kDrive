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

#include "testfolderwatcherwin.h"

#include "config.h"
#include "requests/parameterscache.h"
#include "test_classes/syncpaltest.h"
#include "test_utility/localtemporarydirectory.h"
#include "test_utility/testhelpers.h"
#include "update_detection/file_system_observer/localfilesystemobserverworker.h"

#include <cstddef>
#include <cstring>
#include <vector>

namespace KDC {

namespace {

class LocalFileSystemObserverWorkerMock : public LocalFileSystemObserverWorker {
    public:
        using LocalFileSystemObserverWorker::LocalFileSystemObserverWorker;

        void setSnapshotValid() { _liveSnapshot.setValid(true); }
        const auto &pendingFileEvents() const { return _pendingFileEvents; }
        int32_t changesDetectedCallCount() const { return _changesDetectedCallCount; }

        ExitInfo changesDetected(const std::list<std::pair<SyncPath, OperationType>> &changes) override {
            ++_changesDetectedCallCount;
            return LocalFileSystemObserverWorker::changesDetected(changes);
        }

    private:
        int32_t _changesDetectedCallCount = 0;
};

// Builds a buffer laid out as ReadDirectoryChangesW returns it: DWORD-aligned FILE_NOTIFY_INFORMATION entries.
class NotifyBufferBuilder {
    public:
        void add(const DWORD action, const std::wstring &fileName) {
            const size_t nameBytes = fileName.size() * sizeof(WCHAR);
            const size_t entryBytes = offsetof(FILE_NOTIFY_INFORMATION, FileName) + nameBytes;
            const size_t entryWords = (entryBytes + sizeof(DWORD) - 1) / sizeof(DWORD);

            const size_t entryIndex = _words.size();
            if (!_words.empty())
                entryAt(_lastEntryIndex)->NextEntryOffset = static_cast<DWORD>((entryIndex - _lastEntryIndex) * sizeof(DWORD));

            _words.resize(entryIndex + entryWords, 0);
            _lastEntryIndex = entryIndex;

            auto *entry = entryAt(entryIndex);
            entry->Action = action;
            entry->FileNameLength = static_cast<DWORD>(nameBytes);
            std::memcpy(entry->FileName, fileName.data(), nameBytes);
        }

        [[nodiscard]] const void *data() const { return _words.data(); }
        [[nodiscard]] DWORD size() const { return static_cast<DWORD>(_words.size() * sizeof(DWORD)); }

    private:
        FILE_NOTIFY_INFORMATION *entryAt(const size_t wordIndex) {
            return reinterpret_cast<FILE_NOTIFY_INFORMATION *>(_words.data() + wordIndex);
        }

        std::vector<DWORD> _words; // DWORD storage guarantees the alignment required by FILE_NOTIFY_INFORMATION.
        size_t _lastEntryIndex = 0;
};

} // namespace

void TestFolderWatcherWin::setUp() {
    TestBase::start();
    (void) ParametersCache::instance(true);
}

void TestFolderWatcherWin::tearDown() {
    ParametersCache::reset();
    TestBase::stop();
}

void TestFolderWatcherWin::testBufferOverflowInvalidatesSnapshot() {
    const LocalTemporaryDirectory tempDir;
    const auto syncPal = std::make_shared<SyncPalTest>(tempDir.path() / "sync.db", KDRIVE_VERSION_STRING, true);
    LocalFileSystemObserverWorkerMock parent(syncPal, "Local File System Observer", "LFSO");
    FolderWatcher_win testObj(&parent, tempDir.path());
    parent.setSnapshotValid();
    syncPal->setRestart(false);

    // With overlapped I/O, an overflow is reported as a successful completion returning 0 bytes.
    CPPUNIT_ASSERT_EQUAL(ExitInfo{ExitCode::Ok}, testObj.processNotifyBuffer(nullptr, 0));
    CPPUNIT_ASSERT(!parent.liveSnapshot().isValid());
    CPPUNIT_ASSERT(syncPal->restart());
    CPPUNIT_ASSERT_EQUAL((int32_t)0, parent.changesDetectedCallCount());
}

void TestFolderWatcherWin::testNotificationsAreDispatchedInOneBatch() {
    const LocalTemporaryDirectory tempDir;
    const auto syncPal = std::make_shared<SyncPalTest>(tempDir.path() / "sync.db", KDRIVE_VERSION_STRING, true);
    LocalFileSystemObserverWorkerMock parent(syncPal, "Local File System Observer", "LFSO");
    FolderWatcher_win testObj(&parent, tempDir.path());

    testhelpers::generateOrEditTestFile(tempDir.path() / "created.txt");
    testhelpers::generateOrEditTestFile(tempDir.path() / "edited.txt");
    testhelpers::generateOrEditTestFile(tempDir.path() / "renamed.txt");

    NotifyBufferBuilder buffer;
    buffer.add(FILE_ACTION_ADDED, L"created.txt");
    buffer.add(FILE_ACTION_MODIFIED, L"edited.txt");
    buffer.add(FILE_ACTION_REMOVED, L"deleted.txt");
    buffer.add(FILE_ACTION_RENAMED_OLD_NAME, L"old.txt");
    buffer.add(FILE_ACTION_RENAMED_NEW_NAME, L"renamed.txt");
    buffer.add(FILE_ACTION_ADDED, L"vanished.txt"); // Does not exist anymore: converted to Delete

    // The snapshot is not valid yet: events are queued in the pending list.
    CPPUNIT_ASSERT(!parent.liveSnapshot().isValid());
    CPPUNIT_ASSERT_EQUAL(ExitInfo{ExitCode::Ok}, testObj.processNotifyBuffer(buffer.data(), buffer.size()));
    CPPUNIT_ASSERT_EQUAL((int32_t)1, parent.changesDetectedCallCount());

    const std::vector<std::pair<SyncName, OperationType>> expected = {
            {Str("created.txt"), OperationType::Create}, {Str("edited.txt"), OperationType::Edit},
            {Str("deleted.txt"), OperationType::Delete}, {Str("old.txt"), OperationType::Move},
            {Str("renamed.txt"), OperationType::Move},   {Str("vanished.txt"), OperationType::Delete}};
    const auto &pending = parent.pendingFileEvents();
    CPPUNIT_ASSERT_EQUAL(expected.size(), pending.size());
    auto pendingIt = pending.begin();
    for (const auto &[name, opType]: expected) {
        CPPUNIT_ASSERT(pendingIt->first.filename() == name);
        CPPUNIT_ASSERT_EQUAL(opType, pendingIt->second);
        ++pendingIt;
    }
}

} // namespace KDC
