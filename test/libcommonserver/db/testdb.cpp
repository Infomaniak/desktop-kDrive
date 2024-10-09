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

#include "testdb.h"
#include "libcommonserver/utility/asserts.h"
#include "libcommonserver/log/log.h"

#define CREATE_TEST_TABLE_ID "testdb1"
#define CREATE_TEST_TABLE              \
    "CREATE TABLE IF NOT EXISTS test(" \
    "id INTEGER PRIMARY KEY,"          \
    "intValue INTEGER,"                \
    "int64Value INTEGER,"              \
    "doubleValue INTEGER,"             \
    "textValue TEXT);"

#define INSERT_TEST_REQUEST_ID "testdb2"
#define INSERT_TEST_REQUEST                                                \
    "INSERT INTO test (id, intValue, int64Value, doubleValue, textValue) " \
    "VALUES (?1, ?2, ?3, ?4, ?5);"

#define UPDATE_TEST_REQUEST_ID "testdb3"
#define UPDATE_TEST_REQUEST                                                     \
    "UPDATE test SET intValue=?1, int64Value=?2, doubleValue=?3, textValue=?4 " \
    "WHERE id=?5;"

#define DELETE_TEST_REQUEST_ID "testdb4"
#define DELETE_TEST_REQUEST \
    "DELETE FROM test "     \
    "WHERE id=?1;"

#define SELECT_TEST_REQUEST_ID "testdb5"
#define SELECT_TEST_REQUEST "SELECT id, intValue, int64Value, doubleValue, textValue FROM test;"

#define DROP_TEST_TABLE_ID "testdb6"
#define DROP_TEST_TABLE "DROP TABLE IF EXISTS test;"

using namespace CppUnit;

namespace KDC {

void TestDb::setUp() {
    bool alreadyExists;
    std::filesystem::path testDbPath = Db::makeDbName(1, 1, 1, 1, alreadyExists);
    std::filesystem::remove(testDbPath);
    _testObj = new MyTestDb(testDbPath);
}

void TestDb::tearDown() {
    delete _testObj;
}

void TestDb::testQueries() {
    CPPUNIT_ASSERT(_testObj->exists());

    const Test test0(0, -4321, 101010101010, 4321.0, "");
    Test test1(1, 1234, 123456789012, 1234.5678, "test 1");
    const Test test2(2, 5678, 987654321098, 8765.4321, "test 2");
    CPPUNIT_ASSERT(_testObj->insertTest(test0));
    CPPUNIT_ASSERT(_testObj->insertTest(test1));
    CPPUNIT_ASSERT(_testObj->insertTest(test2));

    std::vector<Test> tests = _testObj->selectTest();
    CPPUNIT_ASSERT_EQUAL(size_t(3), tests.size());

    CPPUNIT_ASSERT_EQUAL(tests[0].intValue, test0.intValue);
    CPPUNIT_ASSERT_EQUAL(tests[0].int64Value, test0.int64Value);
    CPPUNIT_ASSERT_EQUAL(tests[0].doubleValue, test0.doubleValue);
    CPPUNIT_ASSERT_EQUAL(tests[0].textValue, test0.textValue);

    CPPUNIT_ASSERT_EQUAL(tests[1].intValue, test1.intValue);
    CPPUNIT_ASSERT_EQUAL(tests[1].int64Value, test1.int64Value);
    CPPUNIT_ASSERT_EQUAL(tests[1].doubleValue, test1.doubleValue);
    CPPUNIT_ASSERT_EQUAL(tests[1].textValue, test1.textValue);

    CPPUNIT_ASSERT_EQUAL(tests[2].intValue, test2.intValue);
    CPPUNIT_ASSERT_EQUAL(tests[2].int64Value, test2.int64Value);
    CPPUNIT_ASSERT_EQUAL(tests[2].doubleValue, test2.doubleValue);
    CPPUNIT_ASSERT_EQUAL(tests[2].textValue, test2.textValue);

    test1.intValue *= 2;
    test1.int64Value *= 2;
    test1.doubleValue *= 2;
    test1.textValue = std::string("test 1.1");
    CPPUNIT_ASSERT(_testObj->updateTest(test1));

    CPPUNIT_ASSERT(_testObj->deleteTest(tests[1].id));

    const std::vector<Test> tests2 = _testObj->selectTest();
    CPPUNIT_ASSERT_EQUAL(size_t(2), tests2.size());

    CPPUNIT_ASSERT_EQUAL(tests2[0].intValue, test0.intValue);
    CPPUNIT_ASSERT_EQUAL(tests2[0].int64Value, test0.int64Value);
    CPPUNIT_ASSERT_EQUAL(tests2[0].doubleValue, test0.doubleValue);
    CPPUNIT_ASSERT_EQUAL(tests2[0].textValue, test0.textValue);

    CPPUNIT_ASSERT_EQUAL(tests2[1].intValue, test2.intValue);
    CPPUNIT_ASSERT_EQUAL(tests2[1].int64Value, test2.int64Value);
    CPPUNIT_ASSERT_EQUAL(tests2[1].doubleValue, test2.doubleValue);
    CPPUNIT_ASSERT_EQUAL(tests2[1].textValue, test2.textValue);
}

void TestDb::testTableExist() {
    bool exist = false;
    CPPUNIT_ASSERT(_testObj->tableExists("test", exist) && exist);
    CPPUNIT_ASSERT(_testObj->tableExists("not_existing_table_name", exist) && !exist);
}

void TestDb::testColumnExist() {
    bool exist = false;
    CPPUNIT_ASSERT(_testObj->columnExists("test", "intValue", exist) && exist);
    CPPUNIT_ASSERT(_testObj->columnExists("test", "not_existing_column_name", exist) && !exist);
    CPPUNIT_ASSERT(_testObj->columnExists("not_existing_table_name", "intValue", exist) && !exist);
    CPPUNIT_ASSERT(_testObj->columnExists("not_existing_table_name", "not_existing_column_name", exist) && !exist);
}

void TestDb::testAddColumnIfMissing() {
    const std::string requestId = "test_request_id";
    std::string request = "ALTER TABLE test ADD COLUMN intValue INTEGER;";
    bool columnAdded = false;
    CPPUNIT_ASSERT(_testObj->addColumnIfMissing("test", "intValue", requestId, request, &columnAdded) && !columnAdded);

    request = "ALTER TABLE test ADD COLUMN intValue2 INTEGER;";
    CPPUNIT_ASSERT(_testObj->addColumnIfMissing("test", "intValue2", requestId, request, &columnAdded) && columnAdded);
}

TestDb::MyTestDb::MyTestDb(const std::filesystem::path &dbPath) : Db(dbPath) {
    if (!checkConnect("3.3.4")) {
        throw std::runtime_error("Cannot open DB!");
    }
    init("3.3.4");
}

TestDb::MyTestDb::~MyTestDb() {
    clear();
}

bool TestDb::MyTestDb::create(bool &retry) {
    int errId = -1;
    std::string error;

    // Test
    ASSERT(queryCreate(CREATE_TEST_TABLE_ID));
    if (!queryPrepare(CREATE_TEST_TABLE_ID, CREATE_TEST_TABLE, false, errId, error)) {
        queryFree(CREATE_TEST_TABLE_ID);
        return sqlFail(CREATE_TEST_TABLE_ID, error);
    }
    if (!queryExec(CREATE_TEST_TABLE_ID, errId, error)) {
        queryFree(CREATE_TEST_TABLE_ID);
        return sqlFail(CREATE_TEST_TABLE_ID, error);
    }
    queryFree(CREATE_TEST_TABLE_ID);

    retry = false;
    return true;
}

bool TestDb::MyTestDb::prepare() {
    int errId = -1;
    std::string error;

    // Test
    ASSERT(queryCreate(INSERT_TEST_REQUEST_ID));
    if (!queryPrepare(INSERT_TEST_REQUEST_ID, INSERT_TEST_REQUEST, false, errId, error)) {
        queryFree(INSERT_TEST_REQUEST_ID);
        return sqlFail(INSERT_TEST_REQUEST_ID, error);
    }

    ASSERT(queryCreate(UPDATE_TEST_REQUEST_ID));
    if (!queryPrepare(UPDATE_TEST_REQUEST_ID, UPDATE_TEST_REQUEST, false, errId, error)) {
        queryFree(UPDATE_TEST_REQUEST_ID);
        return sqlFail(UPDATE_TEST_REQUEST_ID, error);
    }

    ASSERT(queryCreate(DELETE_TEST_REQUEST_ID));
    if (!queryPrepare(DELETE_TEST_REQUEST_ID, DELETE_TEST_REQUEST, false, errId, error)) {
        queryFree(DELETE_TEST_REQUEST_ID);
        return sqlFail(DELETE_TEST_REQUEST_ID, error);
    }

    ASSERT(queryCreate(SELECT_TEST_REQUEST_ID));
    if (!queryPrepare(SELECT_TEST_REQUEST_ID, SELECT_TEST_REQUEST, false, errId, error)) {
        queryFree(SELECT_TEST_REQUEST_ID);
        return sqlFail(SELECT_TEST_REQUEST_ID, error);
    }

    return true;
}

bool TestDb::MyTestDb::upgrade(const std::string & /*fromVersion*/, const std::string & /*toVersion*/) {
    return true;
}

bool TestDb::MyTestDb::clear() {
    // Drop test table
    int errId = -1;
    std::string error;

    ASSERT(queryCreate(DROP_TEST_TABLE_ID));
    if (!queryPrepare(DROP_TEST_TABLE_ID, DROP_TEST_TABLE, false, errId, error)) {
        queryFree(DROP_TEST_TABLE_ID);
        return sqlFail(DROP_TEST_TABLE_ID, error);
    }
    if (!queryExec(DROP_TEST_TABLE_ID, errId, error)) {
        queryFree(DROP_TEST_TABLE_ID);
        return sqlFail(DROP_TEST_TABLE_ID, error);
    }
    queryFree(DROP_TEST_TABLE_ID);

    return true;
}

bool TestDb::MyTestDb::insertTest(const Test &test) {
    int64_t rowId = -1;
    int errId = -1;
    std::string error;

    ASSERT(queryResetAndClearBindings(INSERT_TEST_REQUEST_ID));
    ASSERT(queryBindValue(INSERT_TEST_REQUEST_ID, 1, test.id));
    ASSERT(queryBindValue(INSERT_TEST_REQUEST_ID, 2, test.intValue));
    ASSERT(queryBindValue(INSERT_TEST_REQUEST_ID, 3, test.int64Value));
    ASSERT(queryBindValue(INSERT_TEST_REQUEST_ID, 4, test.doubleValue));
    ASSERT(queryBindValue(INSERT_TEST_REQUEST_ID, 5, test.textValue));
    if (!queryExecAndGetRowId(INSERT_TEST_REQUEST_ID, rowId, errId, error)) {
        LOG_WARN(_logger, "Error running query:" << INSERT_TEST_REQUEST_ID);
        return false;
    }

    return true;
}

bool TestDb::MyTestDb::updateTest(const Test &test) {
    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(INSERT_TEST_REQUEST_ID));
    ASSERT(queryBindValue(UPDATE_TEST_REQUEST_ID, 1, test.intValue));
    ASSERT(queryBindValue(UPDATE_TEST_REQUEST_ID, 2, test.int64Value));
    ASSERT(queryBindValue(UPDATE_TEST_REQUEST_ID, 3, test.doubleValue));
    ASSERT(queryBindValue(UPDATE_TEST_REQUEST_ID, 4, test.textValue));
    ASSERT(queryBindValue(UPDATE_TEST_REQUEST_ID, 5, test.id));
    if (!queryExec(UPDATE_TEST_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query:" << UPDATE_TEST_REQUEST_ID);
        return false;
    }

    return true;
}

bool TestDb::MyTestDb::deleteTest(int64_t id) {
    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(DELETE_TEST_REQUEST_ID));
    ASSERT(queryBindValue(DELETE_TEST_REQUEST_ID, 1, id));
    if (!queryExec(DELETE_TEST_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query:" << DELETE_TEST_REQUEST_ID);
        return false;
    }

    return true;
}

std::vector<TestDb::Test> TestDb::MyTestDb::selectTest() {
    std::vector<TestDb::Test> tests;

    for (;;) {
        bool hasData;
        if (!queryNext(SELECT_TEST_REQUEST_ID, hasData)) {
            LOG_DEBUG(_logger, "Error getting query result: " << SELECT_TEST_REQUEST_ID);
            return std::vector<TestDb::Test>();
        }
        if (!hasData) {
            break;
        }
        int64_t value1 = -1;
        int value2 = -1;
        int64_t value3 = -1;
        double value4 = -1.0;
        std::string value5;
        ASSERT(queryInt64Value(SELECT_TEST_REQUEST_ID, 0, value1));
        ASSERT(queryIntValue(SELECT_TEST_REQUEST_ID, 1, value2));
        ASSERT(queryInt64Value(SELECT_TEST_REQUEST_ID, 2, value3));
        ASSERT(queryDoubleValue(SELECT_TEST_REQUEST_ID, 3, value4));
        ASSERT(queryStringValue(SELECT_TEST_REQUEST_ID, 4, value5));
        Test test(value1, value2, value3, value4, value5);
        tests.push_back(test);
    }

    return tests;
}

TestDb::Test::Test(int64_t id, int intValue, int64_t int64Value, double doubleValue, const std::string &textValue) :
    id(id), intValue(intValue), int64Value(int64Value), doubleValue(doubleValue), textValue(textValue) {}

} // namespace KDC
