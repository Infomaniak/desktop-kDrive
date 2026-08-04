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

#include "testchangejournal.h"

#include "test_utility/localtemporarydirectory.h"
#include "test_utility/testhelpers.h"

#include "update_detection/file_system_observer/changeJournal/changeJournal_win.h"

#include <algorithm>

using namespace CppUnit;

namespace KDC {

// Helper: find an entry matching a given leaf name in a list of journal entries.
static bool containsFileName(std::list<ChangeJournalEntry> &entries, ChangeJournal& journal, const SyncName &name) {
    return std::any_of(entries.begin(), entries.end(), [&name, &journal](ChangeJournalEntry &entry) {
        if (entry.fileName.empty()) journal.populateEntryCurrentPath(entry, false);
        return entry.fileName == name;
    });
}

void TestChangeJournal::testCursorRoundTrip() {
    const uint64_t journalId = 0x1234567890ABCDEFull;
    const int64_t usn = 987654321012345ll;

    const std::string cursor = ChangeJournal::makeCursor(journalId, usn);
    CPPUNIT_ASSERT(!cursor.empty());

    uint64_t parsedJournalId = 0;
    int64_t parsedUsn = 0;
    CPPUNIT_ASSERT(ChangeJournal::parseCursor(cursor, parsedJournalId, parsedUsn));
    CPPUNIT_ASSERT_EQUAL(journalId, parsedJournalId);
    CPPUNIT_ASSERT_EQUAL(usn, parsedUsn);
}

void TestChangeJournal::testParseCursorRejectsMalformed() {
    uint64_t journalId = 0;
    int64_t usn = 0;

    CPPUNIT_ASSERT(!ChangeJournal::parseCursor("", journalId, usn));
    CPPUNIT_ASSERT(!ChangeJournal::parseCursor("garbage", journalId, usn));
    CPPUNIT_ASSERT(!ChangeJournal::parseCursor("USNJRNL:", journalId, usn));
    CPPUNIT_ASSERT(!ChangeJournal::parseCursor("USNJRNL:123", journalId, usn));
    CPPUNIT_ASSERT(!ChangeJournal::parseCursor("WRONG:123:456", journalId, usn));
    CPPUNIT_ASSERT(!ChangeJournal::parseCursor("USNJRNL:123:-5", journalId, usn));
}

void TestChangeJournal::testReasonFlags() {
    ChangeJournalEntry entry;
    entry.reason = ChangeJournalReason::FileCreate | ChangeJournalReason::Close;

    CPPUNIT_ASSERT(entry.hasReason(ChangeJournalReason::FileCreate));
    CPPUNIT_ASSERT(entry.hasReason(ChangeJournalReason::Close));
    CPPUNIT_ASSERT(!entry.hasReason(ChangeJournalReason::FileDelete));
}

void TestChangeJournal::testSupportsChangeJournal() {
    const LocalTemporaryDirectory tmpDir("changejournal");
    ChangeJournal journal(tmpDir.path());

    // We cannot assume the CI volume has an active journal, but the call must not crash and must be consistent
    // with the journal identifier being available.
    const bool supported = journal.supportsChangeJournal();
    if (supported) {
        CPPUNIT_ASSERT(journal.journalId() != 0);
    }
}

void TestChangeJournal::testCurrentCursor() {
    const LocalTemporaryDirectory tmpDir("changejournal");
    ChangeJournal journal(tmpDir.path());
    if (!journal.supportsChangeJournal()) return; // No journal on this volume, skip.

    std::string cursor;
    CPPUNIT_ASSERT(journal.currentCursor(cursor));
    CPPUNIT_ASSERT(!cursor.empty());

    uint64_t journalId = 0;
    int64_t usn = 0;
    CPPUNIT_ASSERT(ChangeJournal::parseCursor(cursor, journalId, usn));
    CPPUNIT_ASSERT_EQUAL(journal.journalId(), journalId);
}

void TestChangeJournal::testGetEntriesDetectsChanges() {
    const LocalTemporaryDirectory tmpDir("changejournal");
    ChangeJournal journal(tmpDir.path());
    if (!journal.supportsChangeJournal()) return; // No journal on this volume, skip.

    std::string cursor;
    CPPUNIT_ASSERT(journal.currentCursor(cursor));

    // Produce a change on the volume.
    const SyncName fileName = Str2SyncName("changejournal_test_file.txt");
    const SyncPath filePath = tmpDir.path() / fileName;
    testhelpers::generateOrEditTestFile(filePath);

    std::list<ChangeJournalEntry> entries;
    std::string nextCursor;
    CPPUNIT_ASSERT(journal.getEntries(cursor, entries, nextCursor));
    CPPUNIT_ASSERT(!nextCursor.empty());

    // The created file must show up in the journal.
    CPPUNIT_ASSERT(containsFileName(entries, journal, fileName));

    // A subsequent call with the returned cursor and no new change must not report the same entry again.
    std::list<ChangeJournalEntry> entries2;
    std::string nextCursor2;
    CPPUNIT_ASSERT(journal.getEntries(nextCursor, entries2, nextCursor2));
    CPPUNIT_ASSERT(!containsFileName(entries2, journal, fileName));
}

void TestChangeJournal::testGetEntriesRejectsForeignJournalId() {
    const LocalTemporaryDirectory tmpDir("changejournal");
    ChangeJournal journal(tmpDir.path());
    if (!journal.supportsChangeJournal()) return; // No journal on this volume, skip.

    // Craft a cursor referencing a journal identifier that does not match the real one.
    const uint64_t foreignJournalId = journal.journalId() ^ 0xFFFFFFFFFFFFFFFFull;
    const std::string foreignCursor = ChangeJournal::makeCursor(foreignJournalId, 0);

    std::list<ChangeJournalEntry> entries;
    std::string nextCursor;
    const ExitInfo exitInfo = journal.getEntries(foreignCursor, entries, nextCursor);
    CPPUNIT_ASSERT_EQUAL(ExitCode::DataError, exitInfo.code());
    CPPUNIT_ASSERT_EQUAL(ExitCause::InvalidArgument, exitInfo.cause());
}

void TestChangeJournal::testGetEntriesRejectsMalformedCursor() {
    const LocalTemporaryDirectory tmpDir("changejournal");
    ChangeJournal journal(tmpDir.path());
    if (!journal.supportsChangeJournal()) return; // No journal on this volume, skip.

    std::list<ChangeJournalEntry> entries;
    std::string nextCursor;
    const ExitInfo exitInfo = journal.getEntries("not-a-valid-cursor", entries, nextCursor);
    CPPUNIT_ASSERT_EQUAL(ExitCode::SystemError, exitInfo.code());
    CPPUNIT_ASSERT_EQUAL(ExitCause::InvalidArgument, exitInfo.cause());
}

void TestChangeJournal::testPopulateEntryPath() {
    const LocalTemporaryDirectory tmpDir("changejournal");
    ChangeJournal journal(tmpDir.path());
    if (!journal.supportsChangeJournal()) return; // No journal on this volume, skip.

    std::string cursor;
    CPPUNIT_ASSERT(journal.currentCursor(cursor));

    const SyncName fileName = Str2SyncName("changejournal_populate.txt");
    const SyncPath filePath = tmpDir.path() / fileName;
    testhelpers::generateOrEditTestFile(filePath);

    std::list<ChangeJournalEntry> entries;
    std::string nextCursor;
    CPPUNIT_ASSERT(journal.getEntries(cursor, entries, nextCursor));

    // Find the entry for our file by resolving each entry's path.
    bool found = false;
    for (auto &entry: entries) {
        if (!journal.populateEntryCurrentPath(entry, true)) continue; // Item might have been deleted concurrently.
        if (entry.path == filePath) {
            CPPUNIT_ASSERT(fileName == entry.fileName);
            found = true;
            break;
        }
    }
    CPPUNIT_ASSERT(found);
}

void TestChangeJournal::testPopulateEntryPathForDeletedItem() {
    const LocalTemporaryDirectory tmpDir("changejournal");
    ChangeJournal journal(tmpDir.path());
    if (!journal.supportsChangeJournal()) return; // No journal on this volume, skip.

    std::string cursor;
    CPPUNIT_ASSERT(journal.currentCursor(cursor));

    // Create then delete a file so the journal records a delete for a now-missing item.
    const SyncName fileName = Str2SyncName("changejournal_deleted.txt");
    const SyncPath filePath = tmpDir.path() / fileName;
    testhelpers::generateOrEditTestFile(filePath);
    std::filesystem::remove(filePath);

    std::list<ChangeJournalEntry> entries;
    std::string nextCursor;
    CPPUNIT_ASSERT(journal.getEntries(cursor, entries, nextCursor));

    // For a delete record, the item cannot be opened anymore, but the path must be rebuilt from the parent directory.
    bool found = false;
    for (auto &entry: entries) {
        if (!entry.hasReason(ChangeJournalReason::FileDelete)) continue;
        CPPUNIT_ASSERT(!journal.populateEntryCurrentPath(entry, true));
        found = true;
        break;
    }
    CPPUNIT_ASSERT(found);
}

// Helper: build a human-readable string from a ChangeJournalReason bitmask.
static std::string reasonToString(ChangeJournalReason reason) {
    struct Flag {
            ChangeJournalReason mask;
            const char *name;
    };
    static constexpr Flag kFlags[] = {
            {ChangeJournalReason::FileCreate, "FileCreate"},
            {ChangeJournalReason::FileDelete, "FileDelete"},
            {ChangeJournalReason::DataOverwrite, "DataOverwrite"},
            {ChangeJournalReason::DataExtend, "DataExtend"},
            {ChangeJournalReason::DataTruncation, "DataTruncation"},
            {ChangeJournalReason::RenameOldName, "RenameOldName"},
            {ChangeJournalReason::RenameNewName, "RenameNewName"},
            {ChangeJournalReason::BasicInfoChange, "BasicInfoChange"},
            {ChangeJournalReason::SecurityChange, "SecurityChange"},
            {ChangeJournalReason::Close, "Close"},
    };
    std::string result;
    for (const auto &flag: kFlags) {
        if ((reason & flag.mask) != ChangeJournalReason::None) {
            if (!result.empty()) result += '|';
            result += flag.name;
        }
    }
    if (result.empty()) result = "Unknown";
    return result;
}

// Helper: print a single journal entry to stdout.
static void printEntry(ChangeJournal &journal, ChangeJournalEntry &entry) {
    journal.populateEntryCurrentPath(entry, true);

    const std::string name = !entry.fileName.empty() ? SyncName2Str(entry.fileName)
                                                      : "<FRN:" + std::to_string(entry.fileReferenceNumber) + ">";
    const std::string path = !entry.path.empty() ? Path2Str(entry.path)
                                                  : "<parentFRN:" + std::to_string(entry.parentFileReferenceNumber) + ">";

    std::cout << "  name=" << name << "  path=" << path << "  reason=" << reasonToString(entry.reason) << '\n';
}

void TestChangeJournal::testMonitorLive() {
    // Observe the whole system drive (C:\) so that any file-system activity shows up.
    ChangeJournal journal(SyncPath(L"C:\\"));
    if (!journal.supportsChangeJournal()) {
        std::cout << "\n[testMonitorLive] No active USN journal on C:\\ — skipping.\n";
        return;
    }

    std::string cursor;
    CPPUNIT_ASSERT(journal.currentCursor(cursor));

    static constexpr int kDurationSeconds = 300;
    static constexpr DWORD kPollIntervalMs = 500;
    static constexpr int kIterations = kDurationSeconds * 1000 / kPollIntervalMs;

    std::cout << "\n--- Live monitor on C:\\ for " << kDurationSeconds << "s (poll every " << kPollIntervalMs << " ms) ---\n";
    std::cout << "Make some file-system changes now...\n";
    std::cout.flush();

    int totalEvents = 0;
    for (int i = 0; i < kIterations; ++i) {
        Sleep(kPollIntervalMs);

        std::list<ChangeJournalEntry> entries;
        std::string nextCursor;
        if (!journal.getEntries(cursor, entries, nextCursor)) break;
        cursor = nextCursor;

        for (auto &entry: entries) {
            printEntry(journal, entry);
            ++totalEvents;
        }
        if (!entries.empty()) std::cout.flush();
    }

    std::cout << "--- End of monitor (" << totalEvents << " event(s) total) ---\n";
}

} // namespace KDC
