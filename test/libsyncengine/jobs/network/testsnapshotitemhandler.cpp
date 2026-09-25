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

#include "testsnapshotitemhandler.h"

#include "jobs/network/kDrive_API/listing/snapshotitemhandler.h"
#include "update_detection/file_system_observer/snapshot/snapshotitem.h"

#include "log/log.h"

#include "libcommon/utility/utility.h"

using namespace CppUnit;

namespace KDC {

static const std::string endOfFileDelimiter("#EOF");

namespace snapshotitem_checker {
std::string makeMessage(const CppUnit::Exception &e) {
    std::string msg = "Details: \n    -" + e.message().details();
    msg += "    -- line: " + e.sourceLine().fileName() + ":" + std::to_string(e.sourceLine().lineNumber());

    return msg;
}

Result compare(const SnapshotItem &lhs, const SnapshotItem &rhs) noexcept {
    try {
        CPPUNIT_ASSERT_EQUAL(lhs.id(), rhs.id());
        CPPUNIT_ASSERT_EQUAL(lhs.parentId(), rhs.parentId());
        CPPUNIT_ASSERT_EQUAL(SyncName2Str(lhs.name()), SyncName2Str(rhs.name()));
        CPPUNIT_ASSERT_EQUAL(lhs.type(), rhs.type());
        CPPUNIT_ASSERT_EQUAL(lhs.size(), rhs.size());
        CPPUNIT_ASSERT_EQUAL(lhs.createdAt(), rhs.createdAt());
        CPPUNIT_ASSERT_EQUAL(lhs.lastModified(), rhs.lastModified());
        CPPUNIT_ASSERT_EQUAL(lhs.canWrite(), rhs.canWrite());
        CPPUNIT_ASSERT_EQUAL(lhs.isLink(), rhs.isLink());
    } catch (const CppUnit::Exception &e) {
        return Result{false, makeMessage(e)};
    }

    return {};
}
} // namespace snapshotitem_checker

void TestSnapshotItemHandler::testUpdateItem() {
    SnapshotItemHandler handler(Log::instance()->getLogger());
    // Regular cases.
    {
        SnapshotItem item;
        CPPUNIT_ASSERT(handler.updateSnapshotItem("0", SnapshotItemHandler::CsvIndexId, item));
        CPPUNIT_ASSERT_EQUAL(NodeId("0"), item.id());

        CPPUNIT_ASSERT(handler.updateSnapshotItem("1", SnapshotItemHandler::CsvIndexParentId, item));
        CPPUNIT_ASSERT_EQUAL(NodeId("1"), item.parentId());

        CPPUNIT_ASSERT(handler.updateSnapshotItem("kDrive2", SnapshotItemHandler::CsvIndexName, item));
        CPPUNIT_ASSERT_EQUAL(std::string("kDrive2"), SyncName2Str(item.name()));

        CPPUNIT_ASSERT(handler.updateSnapshotItem("file", SnapshotItemHandler::CsvIndexType, item));
        CPPUNIT_ASSERT_EQUAL(NodeType::File, item.type());
        CPPUNIT_ASSERT(handler.updateSnapshotItem("dir", SnapshotItemHandler::CsvIndexType, item));
        CPPUNIT_ASSERT_EQUAL(NodeType::Directory, item.type());

        CPPUNIT_ASSERT(handler.updateSnapshotItem("1000", SnapshotItemHandler::CsvIndexSize, item));
        CPPUNIT_ASSERT_EQUAL(int64_t(1000), item.size());

        CPPUNIT_ASSERT(handler.updateSnapshotItem("123", SnapshotItemHandler::CsvIndexCreatedAt, item));
        CPPUNIT_ASSERT_EQUAL(SyncTime(123), item.createdAt());

        CPPUNIT_ASSERT(handler.updateSnapshotItem("-2082841200", SnapshotItemHandler::CsvIndexCreatedAt, item));
        CPPUNIT_ASSERT_EQUAL(SyncTime(-2082841200), item.createdAt());

        CPPUNIT_ASSERT(handler.updateSnapshotItem("124", SnapshotItemHandler::CsvIndexModtime, item));
        CPPUNIT_ASSERT_EQUAL(SyncTime(124), item.lastModified());
        CPPUNIT_ASSERT(handler.updateSnapshotItem("-1", SnapshotItemHandler::CsvIndexModtime,
                                                  item)); // We can have negative values! (for dates before 1970)
        CPPUNIT_ASSERT_EQUAL(int64_t(-1), item.lastModified());

        CPPUNIT_ASSERT(handler.updateSnapshotItem("1", SnapshotItemHandler::CsvIndexCanWrite, item));
        CPPUNIT_ASSERT_EQUAL(true, item.canWrite());
        CPPUNIT_ASSERT(handler.updateSnapshotItem("0", SnapshotItemHandler::CsvIndexCanWrite, item));
        CPPUNIT_ASSERT_EQUAL(false, item.canWrite());
        CPPUNIT_ASSERT(handler.updateSnapshotItem("X", SnapshotItemHandler::CsvIndexCanWrite, item));
        CPPUNIT_ASSERT_EQUAL(false, item.canWrite());

        CPPUNIT_ASSERT(handler.updateSnapshotItem("0", SnapshotItemHandler::CsvIndexIsLink, item));
        CPPUNIT_ASSERT_EQUAL(false, item.isLink());
        CPPUNIT_ASSERT(handler.updateSnapshotItem("X", SnapshotItemHandler::CsvIndexIsLink, item));
        CPPUNIT_ASSERT_EQUAL(false, item.isLink());
    }

    // Invalid sizes.
    {
        SnapshotItem item;
        CPPUNIT_ASSERT(!handler.updateSnapshotItem("Invalid Size! Integer representation expected",
                                                   SnapshotItemHandler::CsvIndexSize, item));
        CPPUNIT_ASSERT_EQUAL(int64_t(0), item.size());
    }
    {
        SnapshotItem item;
        CPPUNIT_ASSERT(!handler.updateSnapshotItem("-1", SnapshotItemHandler::CsvIndexSize, item));
        CPPUNIT_ASSERT_EQUAL(int64_t(-1), item.size());
    }
    {
        SnapshotItem item;
        CPPUNIT_ASSERT(!handler.updateSnapshotItem(std::string(100, '9'), SnapshotItemHandler::CsvIndexSize, item));
        CPPUNIT_ASSERT_EQUAL(int64_t(0), item.size());
    }

    // Invalid dates.
    {
        SnapshotItem item;
        CPPUNIT_ASSERT(!handler.updateSnapshotItem("Invalid date! Integer representation expected",
                                                   SnapshotItemHandler::CsvIndexCreatedAt, item));
        CPPUNIT_ASSERT_EQUAL(int64_t(0), item.createdAt());
    }
    {
        SnapshotItem item;
        CPPUNIT_ASSERT(!handler.updateSnapshotItem(std::string(100, '9'), SnapshotItemHandler::CsvIndexCreatedAt, item));
        CPPUNIT_ASSERT_EQUAL(int64_t(0), item.createdAt());
    }
    {
        SnapshotItem item;
        CPPUNIT_ASSERT(!handler.updateSnapshotItem("Invalid date! Integer representation expected",
                                                   SnapshotItemHandler::CsvIndexModtime, item));
        CPPUNIT_ASSERT_EQUAL(int64_t(0), item.lastModified());
    }
    {
        SnapshotItem item;
        CPPUNIT_ASSERT(!handler.updateSnapshotItem(std::string(100, '9'), SnapshotItemHandler::CsvIndexModtime, item));
        CPPUNIT_ASSERT_EQUAL(int64_t(0), item.lastModified());
    }
}

// Turn the name string into the form that is returned from the backend.
// Reference : https://www.ietf.org/rfc/rfc4180.txt
std::string toCsvString(const std::string &name) {
    std::stringstream ss;
    bool encloseInDoubleQuotes = false;
    bool prevCharBackslash = false;

    for (char c: name) {
        if (c == '"') {
            if (!prevCharBackslash) { // If a double quote is preceded by a comma, do not insert a second double quote.
                encloseInDoubleQuotes = true;
                ss << '"'; // Insert 2 double quotes instead of one
            }
        }

        if (c == ',' || c == '\n') {
            encloseInDoubleQuotes = true;
        }

        ss << c;
        prevCharBackslash = c == '\\';
    }

    std::string output;
    if (encloseInDoubleQuotes) output += '"';
    output += ss.str();
    if (encloseInDoubleQuotes) output += '"';
    return output;
}

void TestSnapshotItemHandler::testToCsvString() {
    // Nothing to change
    std::string actual = toCsvString(R"(test)");
    std::string expected = R"(test)";
    CPPUNIT_ASSERT_EQUAL(expected, actual);

    // Double quote enclosed if file name contains a comma
    actual = toCsvString(R"(te,st)");
    expected = R"("te,st")";
    CPPUNIT_ASSERT_EQUAL(expected, actual);

    // Name contains double quotes
    actual = toCsvString(R"(test"test"test)");
    expected = R"("test""test""test")";
    CPPUNIT_ASSERT_EQUAL(expected, actual);
    actual = toCsvString(R"("test")");
    expected = R"("""test""")";
    CPPUNIT_ASSERT_EQUAL(expected, actual);

    // Name contains line return
    actual = toCsvString(R"(test
test)");
    expected = R"("test
test")";
    CPPUNIT_ASSERT_EQUAL(expected, actual);

    // Name contains escaped double quote (\")
    actual = toCsvString(R"(te\"st)");
    expected = R"(te\"st)";
    CPPUNIT_ASSERT_EQUAL(expected, actual);
}

void TestSnapshotItemHandler::testGetItem() {
    // A single line to define an item: failure
    {
        SnapshotItem item;
        bool ignore = true;
        bool error = true;
        bool eof = true;
        std::stringstream ss;
        ss << "id,parent_id,name,type,size,created_at,last_modified_at,can_write,is_link";
        SnapshotItemHandler handler(Log::instance()->getLogger());
        CPPUNIT_ASSERT(!handler.getItem(item, ss, error, ignore, eof));
        CPPUNIT_ASSERT(!ignore);
        CPPUNIT_ASSERT(!error);
    }

    // No quotes within the snapshot item name: success, the item won't be ignored
    {
        SnapshotItem item;
        bool ignore = false;
        bool error = false;
        bool eof = false;
        std::stringstream ss;
        ss << "id,parent_id,name,type,size,created_at,last_modified_at,can_write,is_link\n"
           << "0,1," << toCsvString("kDrive2") << ",dir,1000,123,124,0,1";
        SnapshotItemHandler handler(Log::instance()->getLogger());
        CPPUNIT_ASSERT(handler.getItem(item, ss, error, ignore, eof));
        CPPUNIT_ASSERT(!ignore);
        CPPUNIT_ASSERT(!error);

        const SnapshotItem expectedItem(NodeId("0"), NodeId("1"), Str2SyncName(std::string("kDrive2")), SyncTime(123),
                                        SyncTime(124), NodeType::Directory, int64_t(1000), true, false, true);

        const auto result = snapshotitem_checker::compare(expectedItem, item);
        CPPUNIT_ASSERT_MESSAGE(result.message, result.success);
    }

    // A pair of double quotes within the snapshot item name: success, the item won't be ignored
    {
        SnapshotItem item;
        bool ignore = false;
        bool error = false;
        bool eof = false;
        std::stringstream ss;
        ss << "id,parent_id,name,type,size,created_at,last_modified_at,can_write,is_link\n"
           << "0,1," << toCsvString(R"("kDrive2")") << ",dir,1000,123,124,0,1,";
        SnapshotItemHandler handler(Log::instance()->getLogger());
        CPPUNIT_ASSERT(handler.getItem(item, ss, error, ignore, eof));
        CPPUNIT_ASSERT(!ignore);
        CPPUNIT_ASSERT(!error);

        const SnapshotItem expectedItem(NodeId("0"), NodeId("1"), Str2SyncName(std::string(R"("kDrive2")")), SyncTime(123),
                                        SyncTime(124), NodeType::Directory, int64_t(1000), true, false, true);

        const auto result = snapshotitem_checker::compare(expectedItem, item);
        CPPUNIT_ASSERT_MESSAGE(result.message, result.success);
    }

    // Two pairs of double quotes within the snapshot item name: success
    {
        SnapshotItem item;
        bool ignore = false;
        bool error = false;
        bool eof = false;
        std::stringstream ss;
        ss << "id,parent_id,name,type,size,created_at,last_modified_at,can_write,is_link\n"
           << "0,1," << toCsvString(R"(""kDrive2"")") << ",dir,1000,123,124,0,1";
        SnapshotItemHandler handler(Log::instance()->getLogger());
        CPPUNIT_ASSERT(handler.getItem(item, ss, error, ignore, eof));
        CPPUNIT_ASSERT(!ignore);
        CPPUNIT_ASSERT(!error);
    }

    // Harmless line return with the item name: success, the item won't be ignored
    {
        SnapshotItem item;
        bool ignore = false;
        bool error = false;
        bool eof = false;
        std::stringstream ss;
        ss << "id,parent_id,name,type,size,created_at,last_modified_at,can_write,is_link\n"
           << "0,1," << toCsvString(R"(kDrive
2)") << ",dir,1000,123,124,1,0,";
        SnapshotItemHandler handler(Log::instance()->getLogger());
        CPPUNIT_ASSERT(handler.getItem(item, ss, error, ignore, eof));
        CPPUNIT_ASSERT(!ignore);
        CPPUNIT_ASSERT(!error);

        const SnapshotItem expectedItem(NodeId("0"), NodeId("1"), Str2SyncName(std::string("kDrive\n2")), SyncTime(123),
                                        SyncTime(124), NodeType::Directory, int64_t(1000), false, true, true);

        const auto result = snapshotitem_checker::compare(expectedItem, item);
        CPPUNIT_ASSERT_MESSAGE(result.message, result.success);
    }

    // Harmful line return within the item name: error
    {
        SnapshotItem item;
        bool ignore = false;
        bool error = false;
        bool eof = false;
        std::stringstream ss;
        ss << "id,parent_id,name,type,size,created_at,last_modified_at,can_write,is_link\n"
           << "0,1," << toCsvString(R"("kDrive
    )");
        SnapshotItemHandler handler(Log::instance()->getLogger());
        CPPUNIT_ASSERT(handler.getItem(item, ss, error, ignore, eof));
        CPPUNIT_ASSERT(!ignore);
        CPPUNIT_ASSERT(error);
    }

    // Missing required field: no error, but the item will be ignored
    {
        SnapshotItem item;
        bool ignore = false;
        bool error = false;
        bool eof = false;
        std::stringstream ss;
        ss << "id,parent_id,name,type,size,created_at,last_modified_at,can_write,is_link\n"
           << "0,1," << toCsvString(R"(kDrive2)") << ",dir,1000,123,124,";
        SnapshotItemHandler handler(Log::instance()->getLogger());
        CPPUNIT_ASSERT(handler.getItem(item, ss, error, ignore, eof));
        CPPUNIT_ASSERT(ignore);
        CPPUNIT_ASSERT(!error);
    }

    // Double quotes within the snapshot item name: success
    {
        SnapshotItem item;
        bool ignore = false;
        bool error = false;
        bool eof = false;
        std::stringstream ss;
        ss << "id,parent_id,name,type,size,created_at,last_modified_at,can_write,is_link\n"
           << "0,1," << toCsvString(R"("test"test")") << ",dir,1000,123,124,0,1";
        SnapshotItemHandler handler(Log::instance()->getLogger());
        CPPUNIT_ASSERT(handler.getItem(item, ss, error, ignore, eof));
        CPPUNIT_ASSERT(!ignore);
        CPPUNIT_ASSERT(!error);
    }

    // A pair of double quotes within the snapshot item name: success
    {
        SnapshotItem item;
        bool ignore = false;
        bool error = false;
        bool eof = false;
        std::stringstream ss;
        ss << "id,parent_id,name,type,size,created_at,last_modified_at,can_write,is_link\n"
           << "0,1," << toCsvString(R"("kDrive2")") << ",dir,1000,123,124,0,1";
        SnapshotItemHandler handler(Log::instance()->getLogger());
        CPPUNIT_ASSERT(handler.getItem(item, ss, error, ignore, eof));
        CPPUNIT_ASSERT(!ignore);
        CPPUNIT_ASSERT(!error);
    }

    // Escaped double quotes within the snapshot item name: no error, but the item will be ignored
    {
        SnapshotItem item;
        bool ignore = false;
        bool error = false;
        bool eof = false;
        std::stringstream ss;
        ss << "id,parent_id,name,type,size,created_at,last_modified_at,can_write,is_link\n"
           << "0,1," << toCsvString(R"(test\"test)") << ",dir,1000,123,124,0,1";
        SnapshotItemHandler handler(Log::instance()->getLogger());
        CPPUNIT_ASSERT(handler.getItem(item, ss, error, ignore, eof));
        CPPUNIT_ASSERT(ignore);
        CPPUNIT_ASSERT(!error);
        // The item fields are parsed before the ignore check, so that the item can be blacklisted by id.
        CPPUNIT_ASSERT_EQUAL(NodeId("0"), item.id());
        CPPUNIT_ASSERT_EQUAL(NodeId("1"), item.parentId());
    }

    // Escaped double quotes within the snapshot item name: no error, but the item will be ignored
    {
        SnapshotItem item;
        bool ignore = false;
        bool error = false;
        bool eof = false;
        std::stringstream ss;
        ss << "id,parent_id,name,type,size,created_at,last_modified_at,can_write,is_link\n"
           << "0,1," << toCsvString(R"(test\"test)") << ",dir,1000,123,124,0,1\n"
           << "0,1," << toCsvString(R"("coucou")") << ",dir,1000,123,124,0,1\n"
           << "0,1,coucou2,dir,1000,123,124,0,1";
        SnapshotItemHandler handler(Log::instance()->getLogger());

        // First line should be ignored because of parsing issue
        CPPUNIT_ASSERT(handler.getItem(item, ss, error, ignore, eof));
        CPPUNIT_ASSERT(ignore);
        CPPUNIT_ASSERT(!error);
        // The item fields are parsed before the ignore check, so that the item can be blacklisted by id.
        CPPUNIT_ASSERT_EQUAL(NodeId("0"), item.id());
        // The other ones should be correctly parsed
        int counter = 0;
        while (handler.getItem(item, ss, error, ignore, eof)) {
            counter++;
            CPPUNIT_ASSERT(!ignore);
            CPPUNIT_ASSERT(!error);
        }
        CPPUNIT_ASSERT_EQUAL(2, counter); // There should be 2 valid items
    }

    // An ignored line must not inherit the id of the previously parsed item: the item is reset before each line is read.
    {
        SnapshotItem item;
        bool ignore = false;
        bool error = false;
        bool eof = false;
        std::stringstream ss;
        ss << "id,parent_id,name,type,size,created_at,last_modified_at,can_write,is_link\n"
           << "1,0,test,dir,1000,123,124,0,1\n"
           << "2,0," << toCsvString(R"(test\"test)") << ",dir,1000,123,124,0,1";
        SnapshotItemHandler handler(Log::instance()->getLogger());

        CPPUNIT_ASSERT(handler.getItem(item, ss, error, ignore, eof));
        CPPUNIT_ASSERT(!ignore);
        CPPUNIT_ASSERT(!error);
        CPPUNIT_ASSERT_EQUAL(NodeId("1"), item.id());

        // The second line is ignored, and the item id is the one of the ignored line, not the one of the previous item.
        CPPUNIT_ASSERT(handler.getItem(item, ss, error, ignore, eof));
        CPPUNIT_ASSERT(ignore);
        CPPUNIT_ASSERT(!error);
        CPPUNIT_ASSERT_EQUAL(NodeId("2"), item.id());
    }

    // End of line test : normal case
    {
        SnapshotItem item;
        bool ignore = false;
        bool error = false;
        bool eof = false;
        std::stringstream ss;
        ss << "id,parent_id,name,type,size,created_at,last_modified_at,can_write,is_link\n"
           << "1,0,test,dir,1000,123,124,0,1\n"
           << endOfFileDelimiter.c_str();
        SnapshotItemHandler handler(Log::instance()->getLogger());
        SnapshotItem lastParsedItem;
        while (handler.getItem(item, ss, error, ignore, eof)) {
            lastParsedItem = item;
        }
        CPPUNIT_ASSERT(!ignore);
        CPPUNIT_ASSERT(!error);
        CPPUNIT_ASSERT(eof);
        // The item is reset when the EOF delimiter is read.
        CPPUNIT_ASSERT(item.id().empty());

        const SnapshotItem expectedItem(NodeId("1"), NodeId("0"), Str2SyncName(std::string("test")), static_cast<SyncTime>(123),
                                        static_cast<SyncTime>(124), NodeType::Directory, static_cast<int64_t>(1000), true, false,
                                        true);
        const auto [success, message] = snapshotitem_checker::compare(expectedItem, lastParsedItem);
        CPPUNIT_ASSERT_MESSAGE(message, success);
    }

    // End of line test : missing EOF delimiter
    {
        SnapshotItem item;
        bool ignore = false;
        bool error = false;
        bool eof = false;
        std::stringstream ss;
        ss << "id,parent_id,name,type,size,created_at,last_modified_at,can_write,is_link\n"
           << "1,0,test,dir,1000,123,124,0,1\n";
        SnapshotItemHandler handler(Log::instance()->getLogger());
        SnapshotItem lastParsedItem;
        while (handler.getItem(item, ss, error, ignore, eof)) {
            lastParsedItem = item;
        }
        CPPUNIT_ASSERT(!ignore);
        CPPUNIT_ASSERT(!error);
        CPPUNIT_ASSERT(!eof);
        // The item is reset when no more line can be read.
        CPPUNIT_ASSERT(item.id().empty());

        const SnapshotItem expectedItem(NodeId("1"), NodeId("0"), Str2SyncName(std::string("test")), static_cast<SyncTime>(123),
                                        static_cast<SyncTime>(124), NodeType::Directory, static_cast<int64_t>(1000), true, false,
                                        true);
        const auto [success, message] = snapshotitem_checker::compare(expectedItem, lastParsedItem);
        CPPUNIT_ASSERT_MESSAGE(message, success);
    }

    // End of line test : EOF delimiter not at the end
    {
        SnapshotItem item;
        bool ignore = false;
        bool error = false;
        bool eof = false;
        std::stringstream ss;
        ss << "id,parent_id,name,type,size,created_at,last_modified_at,can_write,is_link\n"
           << "1,0,test,dir,1000,123,124,0,1\n"
           << endOfFileDelimiter.c_str() << "\n"
           << "2,0,test2,dir,1000,123,124,0,1";
        SnapshotItemHandler handler(Log::instance()->getLogger());
        CPPUNIT_ASSERT(handler.getItem(item, ss, error, ignore, eof));
        CPPUNIT_ASSERT(!ignore);
        CPPUNIT_ASSERT(!error);
        CPPUNIT_ASSERT(!eof);
        {
            const SnapshotItem expectedItem(NodeId("1"), NodeId("0"), Str2SyncName(std::string("test")),
                                            static_cast<SyncTime>(123), static_cast<SyncTime>(124), NodeType::Directory,
                                            static_cast<int64_t>(1000), true, false, true);
            const auto [success, message] = snapshotitem_checker::compare(expectedItem, item);
            CPPUNIT_ASSERT_MESSAGE(message, success);
        }

        while (handler.getItem(item, ss, error, ignore, eof)) {
            // Nothing to do, just read the whole file
        }
        CPPUNIT_ASSERT(!ignore);
        CPPUNIT_ASSERT(!error);
        CPPUNIT_ASSERT(eof);
        // The item has been reset when the EOF delimiter was read: the item following the delimiter is not parsed.
        CPPUNIT_ASSERT(item.id().empty());
    }

    // The creation_at value is missing: should be interpreted as 0.
    {
        SnapshotItem item;
        bool ignore = false;
        bool error = false;
        bool eof = false;
        std::stringstream ss;
        ss << "id,parent_id,name,type,size,created_at,last_modified_at,can_write,is_link\n"
           << "0,1," << toCsvString("kDrive2") << ",dir,1000,,124,0,1";
        SnapshotItemHandler handler(Log::instance()->getLogger());
        CPPUNIT_ASSERT(handler.getItem(item, ss, error, ignore, eof));
        CPPUNIT_ASSERT(!ignore);
        CPPUNIT_ASSERT(!error);

        CPPUNIT_ASSERT_EQUAL(SyncTime{0}, item.createdAt());
    }
}

void TestSnapshotItemHandler::testGetItemWithCorruptedItem() {
    // Real-world CSV replies containing a corrupted item (id 2891437) whose name contains an escaped double quote.
    // The corrupted item spans 3 physical lines. It must be ignored, and the valid items around it must be parsed
    // whatever their position in the reply.
    const std::string commonDocumentsLine = R"(3,1,"Common documents",dir,,1627909284,1779373659,,)";
    const std::string symlinkLine = "2891434,1,symlink_to_folder_outside_sync_dir,file,17,1789713856,1789713856,1,1";
    const std::string myVirusLine = "2891435,1,myVirus.txt,file,14,1789716406,1789716417,1,";
    const std::string corruptedItemLines = R"csv(2891437,1,"A\"
2891435,2891434,myVirus.txt,file,,1786459004,1788263590,1,
Z",file,4,1789735691,1789735698,1,)csv";

    const SnapshotItem expectedCommonDocuments(NodeId("3"), NodeId("1"), Str2SyncName(std::string("Common documents")),
                                               static_cast<SyncTime>(1627909284), static_cast<SyncTime>(1779373659),
                                               NodeType::Directory, static_cast<int64_t>(0), false, false, true);
    const SnapshotItem expectedSymlink(NodeId("2891434"), NodeId("1"),
                                       Str2SyncName(std::string("symlink_to_folder_outside_sync_dir")),
                                       static_cast<SyncTime>(1789713856), static_cast<SyncTime>(1789713856), NodeType::File,
                                       static_cast<int64_t>(17), true, true, true);
    const SnapshotItem expectedMyVirus(NodeId("2891435"), NodeId("1"), Str2SyncName(std::string("myVirus.txt")),
                                       static_cast<SyncTime>(1789716406), static_cast<SyncTime>(1789716417), NodeType::File,
                                       static_cast<int64_t>(14), false, true, true);
    // The second physical line of the corrupted item is parsed as a separate valid item.
    const SnapshotItem expectedMyVirusWithFileParent(
            NodeId("2891435"), NodeId("2891434"), Str2SyncName(std::string("myVirus.txt")), static_cast<SyncTime>(1786459004),
            static_cast<SyncTime>(1788263590), NodeType::File, static_cast<int64_t>(0), false, true, true);

    struct ParsedItem {
            bool ignore{false};
            SnapshotItem item;
    };
    const auto parseCsvReply = [](const std::string &body) {
        std::stringstream ss;
        ss << "id,parent_id,name,type,size,created_at,last_modified_at,can_write,is_link\n" << body << "\n" << endOfFileDelimiter;
        SnapshotItemHandler handler(Log::instance()->getLogger());
        std::vector<ParsedItem> parsedItems;
        SnapshotItem item;
        bool error = false;
        bool ignore = false;
        bool eof = false;
        while (handler.getItem(item, ss, error, ignore, eof)) {
            parsedItems.emplace_back(ignore, item);
        }
        CPPUNIT_ASSERT(!error);
        CPPUNIT_ASSERT(eof);
        return parsedItems;
    };
    const auto checkParsedItem = [](const ParsedItem &parsedItem, bool expectedIgnore, const SnapshotItem &expectedItem,
                                    const NodeId &expectedIgnoredId = NodeId(), const NodeId &expectedIgnoredParentId = NodeId(),
                                    const SyncName &expectedIgnoredName = {}) {
        CPPUNIT_ASSERT_EQUAL(expectedIgnore, parsedItem.ignore);
        if (expectedIgnore) {
            // The item fields are parsed before the ignore check, so that an error can be reported for this item.
            CPPUNIT_ASSERT_EQUAL(expectedIgnoredId, parsedItem.item.id());
            CPPUNIT_ASSERT_EQUAL(expectedIgnoredParentId, parsedItem.item.parentId());
            CPPUNIT_ASSERT_EQUAL(expectedIgnoredName, parsedItem.item.name());
        } else {
            const auto result = snapshotitem_checker::compare(expectedItem, parsedItem.item);
            CPPUNIT_ASSERT_MESSAGE(result.message, result.success);
        }
    };

    // Case 1: symlink, valid item, corrupted item
    {
        const auto parsedItems =
                parseCsvReply(commonDocumentsLine + "\n" + symlinkLine + "\n" + myVirusLine + "\n" + corruptedItemLines);
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(6), parsedItems.size());
        checkParsedItem(parsedItems[0], false, expectedCommonDocuments);
        checkParsedItem(parsedItems[1], false, expectedSymlink);
        checkParsedItem(parsedItems[2], false, expectedMyVirus);
        checkParsedItem(parsedItems[3], true, SnapshotItem(), NodeId("2891437"), NodeId("1"), Str2SyncName(std::string("A\\")));
        checkParsedItem(parsedItems[4], false, expectedMyVirusWithFileParent);
        checkParsedItem(parsedItems[5], true, SnapshotItem());
    }

    // Case 2: symlink, corrupted item, valid item
    {
        const auto parsedItems =
                parseCsvReply(commonDocumentsLine + "\n" + symlinkLine + "\n" + corruptedItemLines + "\n" + myVirusLine);
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(6), parsedItems.size());
        checkParsedItem(parsedItems[0], false, expectedCommonDocuments);
        checkParsedItem(parsedItems[1], false, expectedSymlink);
        checkParsedItem(parsedItems[2], true, SnapshotItem(), NodeId("2891437"), NodeId("1"), Str2SyncName(std::string("A\\")));
        checkParsedItem(parsedItems[3], false, expectedMyVirusWithFileParent);
        checkParsedItem(parsedItems[4], true, SnapshotItem());
        checkParsedItem(parsedItems[5], false, expectedMyVirus);
    }

    // Case 3: corrupted item, symlink, valid item
    {
        const auto parsedItems =
                parseCsvReply(commonDocumentsLine + "\n" + corruptedItemLines + "\n" + symlinkLine + "\n" + myVirusLine);
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(6), parsedItems.size());
        checkParsedItem(parsedItems[0], false, expectedCommonDocuments);
        checkParsedItem(parsedItems[1], true, SnapshotItem(), NodeId("2891437"), NodeId("1"), Str2SyncName(std::string("A\\")));
        checkParsedItem(parsedItems[2], false, expectedMyVirusWithFileParent);
        checkParsedItem(parsedItems[3], true, SnapshotItem());
        checkParsedItem(parsedItems[4], false, expectedSymlink);
        checkParsedItem(parsedItems[5], false, expectedMyVirus);
    }

    // Case 4: corrupted item, valid item, symlink
    {
        const auto parsedItems =
                parseCsvReply(commonDocumentsLine + "\n" + corruptedItemLines + "\n" + myVirusLine + "\n" + symlinkLine);
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(6), parsedItems.size());
        checkParsedItem(parsedItems[0], false, expectedCommonDocuments);
        checkParsedItem(parsedItems[1], true, SnapshotItem(), NodeId("2891437"), NodeId("1"), Str2SyncName(std::string("A\\")));
        checkParsedItem(parsedItems[2], false, expectedMyVirusWithFileParent);
        checkParsedItem(parsedItems[3], true, SnapshotItem());
        checkParsedItem(parsedItems[4], false, expectedMyVirus);
        checkParsedItem(parsedItems[5], false, expectedSymlink);
    }
}

} // namespace KDC
