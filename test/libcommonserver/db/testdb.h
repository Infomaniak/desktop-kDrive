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

#include "testincludes.h"
#include "libcommonserver/db/db.h"

#include <filesystem>

using namespace CppUnit;

namespace KDC {

class TestDb : public CppUnit::TestFixture {
        CPPUNIT_TEST_SUITE(TestDb);
        CPPUNIT_TEST(testQueries);
        CPPUNIT_TEST(testTableExist);
        CPPUNIT_TEST(testColumnExist);
        CPPUNIT_TEST(testAddColumnIfMissing);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override;
        void tearDown() override;

    protected:
        void testQueries();
        void testTableExist();
        void testColumnExist();
        void testAddColumnIfMissing();

    private:
        struct Test {
                Test(int64_t id, int intValue, int64_t int64Value, double doubleValue, const std::string &textValue);

                int64_t id;
                int intValue;
                int64_t int64Value;
                double doubleValue;
                std::string textValue;
        };

        class MyTestDb : public Db {
            public:
                MyTestDb(const std::filesystem::path &dbPath);
                ~MyTestDb();

                bool create(bool &retry) override;
                bool prepare() override;
                bool upgrade(const std::string &fromVersion, const std::string &toVersion) override;

                bool clear();

                bool insertTest(const Test &test);
                bool updateTest(const Test &test);
                bool deleteTest(int64_t id);
                std::vector<Test> selectTest();
        };

        MyTestDb *_testObj;
};

} // namespace KDC
