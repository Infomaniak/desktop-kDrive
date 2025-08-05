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

#include "testsnapshotitemhandler.h"

#include "jobs/network/API_v2/listing/snapshotitemhandler.h"
#include "libcommonserver/log/log.h"
#include "update_detection/file_system_observer/snapshot/snapshotitem.h"

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
    };

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
        // The other ones should be correctly parsed
        int counter = 0;
        while (handler.getItem(item, ss, error, ignore, eof)) {
            counter++;
            CPPUNIT_ASSERT(!ignore);
            CPPUNIT_ASSERT(!error);
        }
        CPPUNIT_ASSERT_EQUAL(2, counter); // There should be 2 valid items
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
        while (handler.getItem(item, ss, error, ignore, eof)) {
            // Nothing to do, just read the whole file
        }
        CPPUNIT_ASSERT(!ignore);
        CPPUNIT_ASSERT(!error);
        CPPUNIT_ASSERT(eof);

        const SnapshotItem expectedItem(NodeId("1"), NodeId("0"), Str2SyncName(std::string("test")), static_cast<SyncTime>(123),
                                        static_cast<SyncTime>(124), NodeType::Directory, static_cast<int64_t>(1000), true, false,
                                        true);
        const auto [success, message] = snapshotitem_checker::compare(expectedItem, item);
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
        while (handler.getItem(item, ss, error, ignore, eof)) {
            // Nothing to do, just read the whole file
        }
        CPPUNIT_ASSERT(!ignore);
        CPPUNIT_ASSERT(!error);
        CPPUNIT_ASSERT(!eof);

        const SnapshotItem expectedItem(NodeId("1"), NodeId("0"), Str2SyncName(std::string("test")), static_cast<SyncTime>(123),
                                        static_cast<SyncTime>(124), NodeType::Directory, static_cast<int64_t>(1000), true, false,
                                        true);
        const auto [success, message] = snapshotitem_checker::compare(expectedItem, item);
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
        {
            const SnapshotItem expectedItem(NodeId("2"), NodeId("0"), Str2SyncName(std::string("test2")),
                                            static_cast<SyncTime>(123), static_cast<SyncTime>(124), NodeType::Directory,
                                            static_cast<int64_t>(1000), true, false, true);
            const auto [success, message] = snapshotitem_checker::compare(expectedItem, item);
            CPPUNIT_ASSERT_MESSAGE(message, !success);
        }
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

} // namespace KDC
