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

#include "libcommon/utility/types.h"
#include "libcommonserver/log/log.h"

#include <cstdint>
#include <list>
#include <string>

#include <windows.h>
#include <winioctl.h>

namespace KDC {

//! Reason flags describing what happened to a file/directory, as reported by the NTFS USN change journal.
//! These map directly onto the USN_REASON_* constants defined by the Windows API.
enum class ChangeJournalReason : uint32_t {
    None = 0x00000000,
    DataOverwrite = 0x00000001,
    DataExtend = 0x00000002,
    DataTruncation = 0x00000004,
    NamedDataOverwrite = 0x00000010,
    NamedDataExtend = 0x00000020,
    NamedDataTruncation = 0x00000040,
    FileCreate = 0x00000100,
    FileDelete = 0x00000200,
    EaChange = 0x00000400,
    SecurityChange = 0x00000800,
    RenameOldName = 0x00001000,
    RenameNewName = 0x00002000,
    IndexableChange = 0x00004000,
    BasicInfoChange = 0x00008000,
    HardLinkChange = 0x00010000,
    CompressionChange = 0x00020000,
    EncryptionChange = 0x00040000,
    ObjectIdChange = 0x00080000,
    ReparsePointChange = 0x00100000,
    StreamChange = 0x00200000,
    TransactedChange = 0x00400000,
    IntegrityChange = 0x00800000,
    Close = 0x80000000
};

inline ChangeJournalReason operator&(ChangeJournalReason a, ChangeJournalReason b) {
    return static_cast<ChangeJournalReason>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline ChangeJournalReason operator|(ChangeJournalReason a, ChangeJournalReason b) {
    return static_cast<ChangeJournalReason>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

//! A single record read from the NTFS USN change journal.
struct ChangeJournalEntry {
        //! Unique file identifier of the item on the volume (NTFS file reference number).
        uint64_t fileReferenceNumber = 0;
        //! File identifier of the parent directory of the item.
        uint64_t parentFileReferenceNumber = 0;
        //! Update Sequence Number of this record.
        int64_t usn = 0;
        //! Time at which the change was recorded (100-ns intervals since 1601, FILETIME representation).
        int64_t timestamp = 0;
        //! Bitmask of the reasons this record was generated.
        ChangeJournalReason reason = ChangeJournalReason::None;
        //! File attributes of the item at the time the record was generated.
        uint32_t fileAttributes = 0;
        //! Name of the item (leaf name only, not the full path).
        SyncName fileName;
        //! Path of the item (full path, drive-letter form, e.g. "C:\\dir\\file.txt"). May be empty if the name could not be
        //! resolved.
        SyncPath path;

        [[nodiscard]] bool hasReason(ChangeJournalReason r) const { return (reason & r) != ChangeJournalReason::None; }
};

//! Independent and self-contained interface to the NTFS USN change journal of the volume hosting a given path.
//!
//! The change journal is a persistent, per-volume log of every change made to files and directories. This class exposes a
//! minimal read-oriented API on top of it:
//!  - it locates the volume hosting the path passed to the constructor,
//!  - it queries the journal metadata (journal identifier and USN boundaries),
//!  - it hands out an opaque cursor that callers persist between runs,
//!  - it returns the list of changes that happened since a previously returned cursor.
//!
//! Security / consistency: a USN journal is identified by a 64-bit identifier that changes every time the journal is deleted
//! and recreated (and USNs are reset). Reusing a cursor generated against a different journal would silently return bogus or
//! partial data. To avoid this, the opaque cursor embeds the journal identifier and every call validates it against the
//! journal currently present on the volume. On mismatch the call fails with a dedicated exit info so that the caller can
//! trigger a full rescan instead of trusting incremental data.
class ChangeJournal {
    public:
        //! \param path Any path located on the volume whose change journal should be observed.
        explicit ChangeJournal(const SyncPath &path);
        ~ChangeJournal();

        ChangeJournal(const ChangeJournal &) = delete;
        ChangeJournal &operator=(const ChangeJournal &) = delete;

        //! Whether the volume hosting the path exposes a usable NTFS change journal.
        [[nodiscard]] bool supportsChangeJournal();

        //! Returns an opaque cursor pointing at the current end of the journal.
        //! Persist it and pass it later to getEntries() to retrieve everything that happened in between.
        //! \param cursor Output opaque cursor.
        [[nodiscard]] ExitInfo currentCursor(std::string &cursor);

        //! Returns all the entries recorded after the position described by fromCursor.
        //! \param fromCursor Opaque cursor previously obtained from currentCursor() or getEntries().
        //! \param entries Output list of change records, ordered by increasing USN.
        //! \param nextCursor Output opaque cursor to be used for the next incremental call.
        [[nodiscard]] ExitInfo getEntries(const std::string &fromCursor, std::list<ChangeJournalEntry> &entries,
                                          std::string &nextCursor);

        //! Journal identifier of the journal currently present on the volume. 0 if unavailable.
        [[nodiscard]] uint64_t journalId() const { return _journalId; }

        //! Resolves the full path of an item from its NTFS file reference number.
        //! Works unprivileged (the file is opened by id with FILE_READ_ATTRIBUTES only).
        //! \param fileReferenceNumber NTFS file reference number of the item.
        //! \param path Output absolute path of the item (drive-letter form, e.g. "C:\\dir\\file.txt").
        [[nodiscard]] ExitInfo pathFromFileReferenceNumber(uint64_t fileReferenceNumber, SyncPath &path);

        //! Populates the fileName and, if requested, the full path of an entry.
        //! Some entries returned by the unprivileged journal read come without a resolvable name; in that case the leaf name
        //! is recovered from the item itself (or from its parent for deleted items) using the file reference numbers.
        //! \param entry Entry to enrich in place.
        //! \param resolveFullPath When true, also resolves entry.path from the reference numbers.
        [[nodiscard]] ExitInfo populateEntryCurrentPath(ChangeJournalEntry &entry, bool resolveFullPath = true);

        //! Serializes a cursor made of a journal identifier and a USN into an opaque string.
        static std::string makeCursor(uint64_t journalId, int64_t usn);
        //! Parses an opaque cursor string. Returns false if the string is malformed.
        static bool parseCursor(const std::string &cursor, uint64_t &journalId, int64_t &usn);

    private:
        //! Opens (if needed) a read handle on the volume hosting the observed path.
        [[nodiscard]] ExitInfo openVolume();
        //! Closes the volume handle.
        void closeVolume();
        //! Queries the journal metadata and refreshes the cached journal identifier.
        //! \param usnJournalData Output journal data structure.
        [[nodiscard]] ExitInfo queryJournal(USN_JOURNAL_DATA_V0 &usnJournalData);
        //! Activates the change journal on the volume if it is not active yet (no-op if already active).
        //! Requires a write-capable volume handle and administrator privileges.
        [[nodiscard]] ExitInfo createJournal();

        SyncPath _path;
        SyncName _volumePath; //!< e.g. L"\\\\.\\C:".
        HANDLE _volumeHandle = INVALID_HANDLE_VALUE;
        uint64_t _journalId = 0;
        log4cplus::Logger _logger;
};

} // namespace KDC
